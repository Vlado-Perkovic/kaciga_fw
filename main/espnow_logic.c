#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "esp_log_buffer.h"
#include "esp_log_level.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "freertos/timers.h" // For TimerHandle_t if needed, not directly used here
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"

#include "espnow_logic.h" // Our header file

static const char *TAG_ESPNOW = "APP_ESPNOW";

// Queue for ESP-NOW events
static QueueHandle_t s_app_espnow_queue = NULL;

// Broadcast MAC address
uint8_t s_app_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Sequence number for broadcast messages
static uint16_t s_app_espnow_broadcast_seq = 0;

// Forward declarations for static functions
static void app_espnow_deinit(app_espnow_send_param_t *send_param);
static void app_espnow_task(void *pvParameter);
static void app_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status); // Matched example's send_cb signature
static void app_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);


/* WiFi should start before using ESPNOW */
static void app_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(APP_ESPNOW_WIFI_MODE) ); // Uses mode from header
    ESP_ERROR_CHECK( esp_wifi_start());

    // Set a fixed channel for reliable broadcast
    // All devices in the ESP-NOW group must be on the same channel.
    ESP_LOGI(TAG_ESPNOW, "Setting Wi-Fi channel to %d", CONFIG_ESPNOW_CHANNEL);
    if (esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        ESP_LOGE(TAG_ESPNOW, "Failed to set Wi-Fi channel. ESP-NOW may be unreliable.");
    }

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE // This Kconfig option needs to be set for LR
    ESP_LOGI(TAG_ESPNOW, "Enabling ESP-NOW Long Range");
    ESP_ERROR_CHECK( esp_wifi_set_protocol(APP_ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
    uint8_t self_mac[ESP_NOW_ETH_ALEN];
    esp_wifi_get_mac(APP_ESPNOW_WIFI_IF, self_mac);
    ESP_LOGI(TAG_ESPNOW, "Wi-Fi initialized. Self MAC: " MACSTR ", Channel: %d", MAC2STR(self_mac), CONFIG_ESPNOW_CHANNEL);
}

/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void app_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) // Matched example
{
    app_espnow_event_t evt;
    app_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL) { // Should not happen with current esp_now_send_cb_t
        ESP_LOGE(TAG_ESPNOW, "Send cb arg error: mac_addr is NULL");
        return;
    }

    evt.id = APP_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(s_app_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG_ESPNOW, "Send send queue fail");
    }
}

static void app_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    app_espnow_event_t evt;
    app_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    const uint8_t * mac_addr = recv_info->src_addr; // Corrected: get src_addr from recv_info
    // const uint8_t * des_addr = recv_info->des_addr; // Not used in this simplified version

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG_ESPNOW, "Receive cb arg error");
        return;
    }
    ESP_LOGE(TAG_ESPNOW, "LEN AAAAAAAA %d", len);

    // ESP_LOGD(TAG_ESPNOW, "Receive ESPNOW data from " MACSTR, MAC2STR(mac_addr));

    evt.id = APP_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL) {
        ESP_LOGE(TAG_ESPNOW, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_app_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG_ESPNOW, "Send receive queue fail");
        free(recv_cb->data); // Free data if cannot queue
    }
}

/* Parse received ESPNOW data. */
static int app_espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, uint32_t *magic, char *out_simple_message, size_t simple_message_len)
{
    app_espnow_data_t *buf = (app_espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;
    ESP_LOG_BUFFER_HEXDUMP(TAG_ESPNOW, buf, data_len, ESP_LOG_WARN);

    if (data_len < sizeof(app_espnow_data_t)) { // Check against our defined struct size
        ESP_LOGE(TAG_ESPNOW, "Receive ESPNOW data too short, len:%d, expected at least %d", data_len, (int)sizeof(app_espnow_data_t));
        return -1;
    }

    *state = buf->state;
    *seq = buf->seq_num;
    *magic = buf->magic;
    strncpy(out_simple_message, buf->simple_message, simple_message_len -1);
    out_simple_message[simple_message_len -1] = '\0'; // Ensure null termination

    crc = buf->crc;
    buf->crc = 0; // Exclude CRC from calculation
    // Calculate CRC over the actual received length, not just the fixed struct size if payload was variable
    // crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);
    // crc_cal = 0;


    // if (crc_cal == crc) {
        return buf->type;
    // }
    // ESP_LOGW(TAG_ESPNOW, "CRC mismatch: calculated 0x%04X, received 0x%04X", crc_cal, crc);
    // return -1;
}

/* Prepare ESPNOW data to be sent. */
// Simplified to always prepare a broadcast message with a simple text payload
static void app_espnow_data_prepare(app_espnow_send_param_t *send_param, const char *message_text)
{
    app_espnow_data_t *buf = (app_espnow_data_t *)send_param->buffer;
    // send_param->len should be sizeof(app_espnow_data_t)

    buf->type = APP_ESPNOW_DATA_BROADCAST;
    buf->state = send_param->state; // Typically 0 for initial broadcast
    buf->seq_num = s_app_espnow_broadcast_seq++;
    buf->magic = send_param->magic; // Use a device-specific magic number or esp_random()
    strncpy(buf->simple_message, message_text, sizeof(buf->simple_message) - 1);
    buf->simple_message[sizeof(buf->simple_message) - 1] = '\0'; // Ensure null termination

    buf->crc = 0; // Clear CRC field before calculation
    // buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}

static void app_espnow_task(void *pvParameter)
{
    app_espnow_event_t evt;
    uint8_t recv_state = 0;
    uint16_t recv_seq = 0;
    uint32_t recv_magic = 0;
    char recv_simple_message[32]; // Buffer for the simple message
    int ret;

    // The send_param is now managed within this task for periodic broadcast
    app_espnow_send_param_t *send_param = malloc(sizeof(app_espnow_send_param_t));
    if (send_param == NULL) {
        ESP_LOGE(TAG_ESPNOW, "Failed to malloc send_param");
        vTaskDelete(NULL);
        return;
    }
    memset(send_param, 0, sizeof(app_espnow_send_param_t));

    send_param->broadcast = true; // We are only broadcasting
    send_param->unicast = false;
    send_param->state = 0;       // Initial state
    send_param->magic = esp_random(); // Each device gets a random magic number
    send_param->delay = CONFIG_ESPNOW_SEND_DELAY; // Delay from Kconfig or default
    send_param->len = sizeof(app_espnow_data_t); // Set length to our struct size
    send_param->buffer = malloc(send_param->len);
    if (send_param->buffer == NULL) {
        ESP_LOGE(TAG_ESPNOW, "Failed to malloc send_param buffer");
        free(send_param);
        vTaskDelete(NULL);
        return;
    }
    memcpy(send_param->dest_mac, s_app_broadcast_mac, ESP_NOW_ETH_ALEN);

    ESP_LOGI(TAG_ESPNOW, "ESP-NOW Task Started. Device Magic: 0x%08"PRIX32, send_param->magic);
    ESP_LOGI(TAG_ESPNOW, "Broadcasting every %d ms", send_param->delay);


    // Initial delay before first send if desired (example task has one)
    // vTaskDelay(pdMS_TO_TICKS(5000)); 

    uint32_t message_counter = 0;

    while (true) {
        // Periodically send a broadcast message
        char current_message[32];
        snprintf(current_message, sizeof(current_message), "Hello #%"PRIu32, message_counter++);
        
        app_espnow_data_prepare(send_param, current_message);

        ESP_LOGI(TAG_ESPNOW, "Attempting to send broadcast: Seq=%u, LEN: %d, Msg='%s'", 
                 ((app_espnow_data_t*)send_param->buffer)->seq_num,
                 send_param->len,
                 ((app_espnow_data_t*)send_param->buffer)->simple_message);

        if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
            ESP_LOGE(TAG_ESPNOW, "Broadcast send error");
            // No deinit here, just log and continue trying
        }
        // The actual send status will be processed from the queue via send_cb

        // Wait for events from send/receive callbacks or timeout for next broadcast
        if (xQueueReceive(s_app_espnow_queue, &evt, pdMS_TO_TICKS(send_param->delay)) == pdTRUE) {
            switch (evt.id) {
                case APP_ESPNOW_SEND_CB:
                {
                    app_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                    if (send_cb->status == ESP_NOW_SEND_SUCCESS) {
                        ESP_LOGI(TAG_ESPNOW, "Broadcast Send SUCCESS to "MACSTR, MAC2STR(send_cb->mac_addr));
                    } else {
                        ESP_LOGW(TAG_ESPNOW, "Broadcast Send FAIL to "MACSTR", Status: %d", MAC2STR(send_cb->mac_addr), send_cb->status);
                    }
                    break;
                }
                case APP_ESPNOW_RECV_CB:
                {
                    app_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
                    ret = app_espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_seq, &recv_magic, recv_simple_message, sizeof(recv_simple_message));
                    
                    if (ret != -1) { // ret is buf->type if CRC is OK
                        ESP_LOGI(TAG_ESPNOW, "Received %s ESPNOW data from: "MACSTR", Seq: %u, Magic: 0x%08"PRIX32", State: %u, Msg: '%s'",
                                 (ret == APP_ESPNOW_DATA_BROADCAST) ? "Broadcast" : "Unicast", // Should always be broadcast
                                 MAC2STR(recv_cb->mac_addr), recv_seq, recv_magic, recv_state, recv_simple_message);
                        
                        // Here you can check if MACSTR(recv_cb->mac_addr) is your own device's MAC
                        // to confirm self-reception.
                        uint8_t self_mac[ESP_NOW_ETH_ALEN];
                        esp_wifi_get_mac(APP_ESPNOW_WIFI_IF, self_mac);
                        if (memcmp(self_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN) == 0) {
                            ESP_LOGI(TAG_ESPNOW, ">>> Self-broadcast received successfully! <<<");
                        }

                    } else {
                        ESP_LOGW(TAG_ESPNOW, "Received corrupted data from: "MACSTR, MAC2STR(recv_cb->mac_addr));
                        ESP_LOG_BUFFER_HEXDUMP(TAG_ESPNOW, recv_cb, sizeof(&recv_cb), ESP_LOG_WARN);
                    }
                    free(recv_cb->data); // Free the malloced data
                    break;
                }
                default:
                    ESP_LOGE(TAG_ESPNOW, "Callback type error: %d", evt.id);
                    break;
            }
        }
        // If xQueueReceive timed out, the loop continues and sends the next broadcast.
        // If not using vTaskDelayUntil, a simple vTaskDelay can be used here if the queue receive is non-blocking (timeout 0)
        // but the example structure uses the queue timeout as the primary delay mechanism between sends.
        // For a fixed periodic broadcast, we can simplify and just use vTaskDelay after sending.
        // The current structure sends, then waits on queue (which also acts as delay).
        // If queue is empty after 'delay', it loops to send again.
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    // Cleanup if task somehow exits (should not happen with while(true))
    app_espnow_deinit(send_param); // Includes freeing send_param->buffer and send_param
    vTaskDelete(NULL);
}

static esp_err_t app_espnow_internal_init(void)
{
    s_app_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(app_espnow_event_t));
    if (s_app_espnow_queue == NULL) {
        ESP_LOGE(TAG_ESPNOW, "Create ESP-NOW event queue fail");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_ESPNOW, "Initializing ESP-NOW service...");
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(app_espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(app_espnow_recv_cb) );

#if CONFIG_ESPNOW_ENABLE_POWER_SAVE // Kconfig option
    ESP_LOGI(TAG_ESPNOW, "Enabling ESP-NOW Power Save Features");
    ESP_ERROR_CHECK( esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW) );
    ESP_ERROR_CHECK( esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL) );
#endif

    ESP_LOGI(TAG_ESPNOW, "Setting ESP-NOW PMK...");
    ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );

    ESP_LOGI(TAG_ESPNOW, "Adding broadcast peer...");
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG_ESPNOW, "Malloc peer information fail");
        vQueueDelete(s_app_espnow_queue);
        s_app_espnow_queue = NULL;
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL; // Use channel from Kconfig or default
    peer->ifidx = APP_ESPNOW_WIFI_IF;
    peer->encrypt = false; // For simple broadcast, no encryption. Set to true and configure LMK if needed.
    // If encrypt = true, peer->lmk must be set: memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
    memcpy(peer->peer_addr, s_app_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);

    return ESP_OK;
}

static void app_espnow_deinit(app_espnow_send_param_t *send_param)
{
    if (send_param) {
        free(send_param->buffer);
        free(send_param);
    }
    if (s_app_espnow_queue) {
        vQueueDelete(s_app_espnow_queue);
        s_app_espnow_queue = NULL;
    }
    esp_now_deinit();
    ESP_LOGI(TAG_ESPNOW, "ESP-NOW Deinitialized.");
}


// Public function to initialize ESP-NOW and start its task
esp_err_t app_espnow_init_and_start_task(void)
{
    // Initialize NVS (needed for Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    app_wifi_init(); // Initialize Wi-Fi

    if (app_espnow_internal_init() != ESP_OK) { // Initialize ESP-NOW internals
        ESP_LOGE(TAG_ESPNOW, "ESP-NOW internal initialization failed.");
        return ESP_FAIL;
    }

    // Create the ESP-NOW task that will handle sending and processing queued events
    // The send_param is now created and managed inside app_espnow_task
    if (xTaskCreate(app_espnow_task, "app_espnow_task", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG_ESPNOW, "Failed to create app_espnow_task");
        app_espnow_deinit(NULL); // Pass NULL as send_param is not created here anymore
        return ESP_FAIL;
    }
    ESP_LOGI(TAG_ESPNOW, "app_espnow_task created.");
    return ESP_OK;
}
