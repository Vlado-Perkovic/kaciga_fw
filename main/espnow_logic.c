#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h" // For NVS initialization
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include <inttypes.h> // For PRIX32

#include "espnow_logic.h" // Your header file

static const char *TAG_ESPNOW_LOGIC = "ESPNOW_FILTER_EXT"; // Updated TAG

static QueueHandle_t s_app_espnow_event_queue = NULL;
uint8_t s_app_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint16_t s_app_espnow_broadcast_seq_num = 0;

// --- Espressif OUI Definitions (Expanded List) ---
// Each OUI is 3 bytes.
static const uint8_t espressif_ouis[][3] = {
    {0x00, 0x4B, 0x12}, {0x04, 0x83, 0x08}, {0x08, 0x3A, 0x8D}, {0x08, 0x3A, 0xF2},
    {0x08, 0xA6, 0xF7}, {0x08, 0xB6, 0x1F}, {0x08, 0xD1, 0xF9}, {0x08, 0xF9, 0xE0},
    {0x0C, 0x4E, 0xA0}, {0x0C, 0x8B, 0x95}, {0x0C, 0xB8, 0x15}, {0x0C, 0xDC, 0x7E},
    {0x10, 0x00, 0x3B}, {0x10, 0x06, 0x1C}, {0x10, 0x20, 0xBA}, {0x10, 0x51, 0xDB},
    {0x10, 0x52, 0x1C}, {0x10, 0x91, 0xA8}, {0x10, 0x97, 0xBD}, {0x10, 0xB4, 0x1D},
    {0x14, 0x2B, 0x2F}, {0x14, 0x33, 0x5C}, {0x18, 0x8B, 0x0E}, {0x18, 0xFE, 0x34},
    {0x1C, 0x69, 0x20}, {0x1C, 0x9D, 0xC2}, {0x20, 0x43, 0xA8}, {0x24, 0x0A, 0xC4},
    {0x24, 0x4C, 0xAB}, {0x24, 0x58, 0x7C}, {0x24, 0x62, 0xAB}, {0x24, 0x6F, 0x28},
    {0x24, 0xA1, 0x60}, {0x24, 0xB2, 0xDE}, {0x24, 0xD7, 0xEB}, {0x24, 0xDC, 0xC3},
    {0x24, 0xEC, 0x4A}, {0x28, 0x37, 0x2F}, {0x28, 0x56, 0x2F}, {0x2C, 0x3A, 0xE8},
    {0x2C, 0xBC, 0xBB}, {0x2C, 0xF4, 0x32}, {0x30, 0x30, 0xF9}, {0x30, 0x83, 0x98},
    {0x30, 0xAE, 0xA4}, {0x30, 0xC6, 0xF7}, {0x30, 0xC9, 0x22}, {0x30, 0xED, 0xA0},
    {0x34, 0x5F, 0x45}, {0x34, 0x85, 0x18}, {0x34, 0x86, 0x5D}, {0x34, 0x94, 0x54},
    {0x34, 0x98, 0x7A}, {0x34, 0xAB, 0x95}, {0x34, 0xB4, 0x72}, {0x34, 0xB7, 0xDA},
    {0x34, 0xCD, 0xB0}, {0x38, 0x18, 0x2B}, {0x3C, 0x61, 0x05}, {0x3C, 0x71, 0xBF},
    {0x3C, 0x84, 0x27}, {0x3C, 0x8A, 0x1F}, {0x3C, 0xE9, 0x0E}, {0x40, 0x22, 0xD8},
    {0x40, 0x4C, 0xCA}, {0x40, 0x91, 0x51}, {0x40, 0xF5, 0x20}, {0x44, 0x17, 0x93},
    {0x44, 0x1D, 0x64}, {0x48, 0x27, 0xE2}, {0x48, 0x31, 0xB7}, {0x48, 0x3F, 0xDA},
    {0x48, 0x55, 0x19}, {0x48, 0xCA, 0x43}, {0x48, 0xE7, 0x29}, {0x4C, 0x11, 0xAE},
    {0x4C, 0x75, 0x25}, {0x4C, 0xC3, 0x82}, {0x4C, 0xEB, 0xD6}, {0x50, 0x02, 0x91},
    {0x50, 0x78, 0x7D}, {0x54, 0x32, 0x04}, {0x54, 0x43, 0xB2}, {0x54, 0x5A, 0xA6},
    {0x58, 0x8C, 0x81}, {0x58, 0xBF, 0x25}, {0x58, 0xCF, 0x79}, {0x5C, 0x01, 0x3B},
    {0x5C, 0xCF, 0x7F}, {0x60, 0x01, 0x94}, {0x60, 0x55, 0xF9}, {0x64, 0xB7, 0x08},
    {0x64, 0xE8, 0x33}, {0x68, 0x25, 0xDD}, {0x68, 0x67, 0x25}, {0x68, 0xB6, 0xB3},
    {0x68, 0xC6, 0x3A}, {0x6C, 0xB4, 0x56}, {0x6C, 0xC8, 0x40}, {0x70, 0x03, 0x9F},
    {0x70, 0x04, 0x1D}, {0x70, 0xB8, 0xF6}, {0x74, 0x4D, 0xBD}, {0x78, 0x1C, 0x3C},
    {0x78, 0x21, 0x84}, {0x78, 0x42, 0x1C}, {0x78, 0xE3, 0x6D}, {0x78, 0xEE, 0x4C},
    {0x7C, 0x2C, 0x67}, {0x7C, 0x73, 0x98}, {0x7C, 0x87, 0xCE}, {0x7C, 0x9E, 0xBD},
    {0x7C, 0xDF, 0xA1}, {0x80, 0x64, 0x6F}, {0x80, 0x65, 0x99}, {0x80, 0x7D, 0x3A},
    {0x80, 0xB5, 0x4E}, {0x80, 0xF3, 0xDA}, {0x84, 0x0D, 0x8E}, {0x84, 0x1F, 0xE8},
    {0x84, 0xCC, 0xA8}, {0x84, 0xF3, 0xEB}, {0x84, 0xF7, 0x03}, {0x84, 0xFC, 0xE6},
    {0x88, 0x13, 0xBF}, {0x8C, 0x4B, 0x14}, {0x8C, 0x4F, 0x00}, {0x8C, 0xAA, 0xB5},
    {0x8C, 0xBF, 0xEA}, {0x8C, 0xCE, 0x4E}, {0x90, 0x15, 0x06}, {0x90, 0x38, 0x0C},
    {0x90, 0x97, 0xD5}, {0x90, 0xE5, 0xB1}, {0x94, 0x3C, 0xC6}, {0x94, 0x54, 0xC5},
    {0x94, 0xA9, 0x90}, {0x94, 0xB5, 0x55}, {0x94, 0xB9, 0x7E}, {0x94, 0xE6, 0x86},
    {0x98, 0x3D, 0xAE}, {0x98, 0x88, 0xE0}, {0x98, 0xA3, 0x16}, {0x98, 0xCD, 0xAC},
    {0x98, 0xF4, 0xAB}, {0x9C, 0x9C, 0x1F}, {0x9C, 0x9E, 0x6E}, {0xA0, 0x20, 0xA6},
    {0xA0, 0x76, 0x4E}, {0xA0, 0x85, 0xE3}, {0xA0, 0xA3, 0xB3}, {0xA0, 0xB7, 0x65},
    {0xA0, 0xDD, 0x6C}, {0xA4, 0x7B, 0x9D}, {0xA4, 0xCF, 0x12}, {0xA4, 0xE5, 0x7C},
    {0xA8, 0x03, 0x2A}, {0xA8, 0x42, 0xE3}, {0xA8, 0x46, 0x74}, {0xA8, 0x48, 0xFA},
    {0xAC, 0x0B, 0xFB}, {0xAC, 0x15, 0x18}, {0xAC, 0x67, 0xB2}, {0xAC, 0xD0, 0x74},
    {0xB0, 0x81, 0x84}, {0xB0, 0xA7, 0x32}, {0xB0, 0xB2, 0x1C}, {0xB4, 0x3A, 0x45},
    {0xB4, 0x8A, 0x0A}, {0xB4, 0xE6, 0x2D}, {0xB8, 0xD6, 0x1A}, {0xB8, 0xF0, 0x09},
    {0xB8, 0xF8, 0x62}, {0xBC, 0xDD, 0xC2}, {0xBC, 0xFF, 0x4D}, {0xC0, 0x49, 0xEF},
    {0xC0, 0x4E, 0x30}, {0xC0, 0x5D, 0x89}, {0xC4, 0x4F, 0x33}, {0xC4, 0x5B, 0xBE},
    {0xC4, 0xD8, 0xD5}, {0xC4, 0xDD, 0x57}, {0xC4, 0xDE, 0xE2}, {0xC8, 0x2B, 0x96},
    {0xC8, 0x2E, 0x18}, {0xC8, 0xC9, 0xA3}, {0xC8, 0xF0, 0x9E}, {0xCC, 0x50, 0xE3},
    {0xCC, 0x7B, 0x5C}, {0xCC, 0x8D, 0xA2}, {0xCC, 0xBA, 0x97}, {0xCC, 0xDB, 0xA7},
    {0xD0, 0xCF, 0x13}, {0xD0, 0xEF, 0x76}, {0xD4, 0x8A, 0xFC}, {0xD4, 0x8C, 0x49},
    {0xD4, 0xD4, 0xDA}, {0xD4, 0xF9, 0x8D}, {0xD8, 0x13, 0x2A}, {0xD8, 0x3B, 0xDA},
    {0xD8, 0xA0, 0x1D}, {0xD8, 0xBC, 0x38}, {0xD8, 0xBF, 0xC0}, {0xD8, 0xF1, 0x5B},
    {0xDC, 0x06, 0x75}, {0xDC, 0x1E, 0xD5}, {0xDC, 0x4F, 0x22}, {0xDC, 0x54, 0x75},
    {0xDC, 0xDA, 0x0C}, {0xE0, 0x5A, 0x1B}, {0xE0, 0x98, 0x06}, {0xE0, 0xE2, 0xE6},
    {0xE4, 0x65, 0xB8}, {0xE4, 0xB0, 0x63}, {0xE4, 0xB3, 0x23}, {0xE8, 0x06, 0x90},
    {0xE8, 0x31, 0xCD}, {0xE8, 0x68, 0xE7}, {0xE8, 0x6B, 0xEA}, {0xE8, 0x9F, 0x6D},
    {0xE8, 0xDB, 0x84}, {0xEC, 0x62, 0x60}, {0xEC, 0x64, 0xC9}, {0xEC, 0x94, 0xCB},
    {0xEC, 0xC9, 0xFF}, {0xEC, 0xDA, 0x3B}, {0xEC, 0xE3, 0x34}, {0xEC, 0xFA, 0xBC},
    {0xF0, 0x08, 0xD1}, {0xF0, 0x24, 0xF9}, {0xF0, 0x9E, 0x9E}, {0xF0, 0xF5, 0xBD},
    {0xF4, 0x12, 0xFA}, {0xF4, 0x65, 0x0B}, {0xF4, 0xCF, 0xA2}, {0xF8, 0xB3, 0xB7},
    {0xFC, 0x01, 0x2C}, {0xFC, 0xB4, 0x67}, {0xFC, 0xE8, 0xC0}, {0xFC, 0xF5, 0xC4}
};
static const int num_espressif_ouis = sizeof(espressif_ouis) / sizeof(espressif_ouis[0]);

// Forward declarations
static void app_espnow_deinit_simplified(void);
static void app_espnow_send_cb_handler_simplified(const uint8_t *mac_addr, esp_now_send_status_t status);
static void app_espnow_recv_cb_handler_simplified(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
static void app_espnow_broadcast_processing_task(void *pvParameter);

// Helper function to check if a MAC address belongs to Espressif
static bool is_espressif_device(const uint8_t *mac_addr) {
    if (mac_addr == NULL) {
        return false;
    }
    for (int i = 0; i < num_espressif_ouis; i++) {
        if (memcmp(mac_addr, espressif_ouis[i], 3) == 0) { // Compare first 3 bytes (OUI)
            return true;
        }
    }
    return false;
}


static void app_wifi_init_simplified(void)
{
    ESP_LOGI(TAG_ESPNOW_LOGIC, "Initializing Wi-Fi (Simplified)...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(APP_ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_ESPNOW_LOGIC, "Setting Wi-Fi channel to %d", CONFIG_ESPNOW_CHANNEL);
    if (esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
        ESP_LOGE(TAG_ESPNOW_LOGIC, "Failed to set Wi-Fi channel %d. ESP-NOW may be unreliable.", CONFIG_ESPNOW_CHANNEL);
    }

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_LOGI(TAG_ESPNOW_LOGIC, "Enabling ESP-NOW Long Range protocol");
    ESP_ERROR_CHECK(esp_wifi_set_protocol(APP_ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
#endif
    uint8_t self_mac[ESP_NOW_ETH_ALEN];
    esp_wifi_get_mac(APP_ESPNOW_WIFI_IF, self_mac);
    uint8_t current_channel;
    esp_wifi_get_channel(&current_channel, NULL);
    ESP_LOGI(TAG_ESPNOW_LOGIC, "Wi-Fi initialized. Self MAC: " MACSTR ", Current Channel (confirmed): %d", MAC2STR(self_mac), current_channel);
}

static void app_espnow_send_cb_handler_simplified(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    app_espnow_event_t evt;
    app_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL) {
        ESP_LOGE(TAG_ESPNOW_LOGIC, "SEND_CB: mac_addr is NULL");
        return;
    }
    evt.id = APP_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (s_app_espnow_event_queue != NULL) {
        if (xQueueSend(s_app_espnow_event_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
            ESP_LOGW(TAG_ESPNOW_LOGIC, "SEND_CB: xQueueSend failed");
        }
    } else {
        ESP_LOGE(TAG_ESPNOW_LOGIC, "SEND_CB: Event queue NULL!");
    }
}

static void app_espnow_recv_cb_handler_simplified(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    app_espnow_event_t evt;
    app_espnow_event_recv_cb_t *recv_cb_evt_data = &evt.info.recv_cb;
    const uint8_t *mac_addr = recv_info->src_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG_ESPNOW_LOGIC, "RECV_CB: Arg error. MAC NULL, data NULL, or len (%d) invalid.", len);
        return;
    }
    if (!is_espressif_device(mac_addr)) {
        uint8_t self_mac[ESP_NOW_ETH_ALEN];
        esp_err_t ret_mac = esp_wifi_get_mac(APP_ESPNOW_WIFI_IF, self_mac);
        // Don't filter out self-broadcasts if they happen to have a non-Espressif OUI (e.g. custom MAC)
        // or if we failed to get our own MAC for comparison.
        if (ret_mac != ESP_OK || memcmp(self_mac, mac_addr, ESP_NOW_ETH_ALEN) != 0) {
             return; // Discard packet
        } else if (ret_mac == ESP_OK) {
            ESP_LOGI(TAG_ESPNOW_LOGIC, "RECV_CB: Processing self-sent packet from MAC " MACSTR " (assuming custom MAC or for debug)", MAC2STR(mac_addr));
        }
    }

    ESP_LOGI(TAG_ESPNOW_LOGIC, "RECV_CB: Received from " MACSTR ", RSSI: %d, len: %d",
             MAC2STR(mac_addr), recv_info->rx_ctrl->rssi, len);
    ESP_LOGD(TAG_ESPNOW_LOGIC, "RECV_CB: Raw data hexdump (len: %d):", len);
    ESP_LOG_BUFFER_HEXDUMP(TAG_ESPNOW_LOGIC, data, len, ESP_LOG_DEBUG);

    // --- OUI Filter for Espressif Devices ---
    // --- End OUI Filter ---

    evt.id = APP_ESPNOW_RECV_CB;
    memcpy(recv_cb_evt_data->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb_evt_data->data = malloc(len);
    if (recv_cb_evt_data->data == NULL) {
        ESP_LOGE(TAG_ESPNOW_LOGIC, "RECV_CB: Malloc for recv_cb_evt_data->data failed (len: %d)", len);
        return;
    }
    memcpy(recv_cb_evt_data->data, data, len);
    recv_cb_evt_data->data_len = len;

    if (s_app_espnow_event_queue != NULL) {
        if (xQueueSend(s_app_espnow_event_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
            ESP_LOGW(TAG_ESPNOW_LOGIC, "RECV_CB: xQueueSend failed, freeing data.");
            free(recv_cb_evt_data->data);
        } else {
            ESP_LOGD(TAG_ESPNOW_LOGIC, "RECV_CB: Queued %d bytes from " MACSTR, len, MAC2STR(mac_addr));
        }
    } else {
        ESP_LOGE(TAG_ESPNOW_LOGIC, "RECV_CB: Event queue NULL! Freeing data.");
        free(recv_cb_evt_data->data);
    }
}

static void app_espnow_prepare_broadcast_data(app_espnow_data_t *data_buf, const char *message_text, uint32_t device_magic) {
    data_buf->type = APP_ESPNOW_DATA_BROADCAST;
    data_buf->state = 0;
    data_buf->seq_num = s_app_espnow_broadcast_seq_num++;
    data_buf->magic = device_magic;
    strncpy(data_buf->simple_message, message_text, sizeof(data_buf->simple_message) - 1);
    data_buf->simple_message[sizeof(data_buf->simple_message) - 1] = '\0';

    data_buf->crc = 0;
    data_buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)data_buf, sizeof(app_espnow_data_t));
}

static void app_espnow_broadcast_processing_task(void *pvParameter)
{
    app_espnow_event_t evt;
    app_espnow_data_t send_data_buffer;
    uint32_t device_magic_number = esp_random();
    uint32_t message_counter = 0;
    uint8_t self_mac[ESP_NOW_ETH_ALEN];

    esp_err_t ret_mac = esp_wifi_get_mac(APP_ESPNOW_WIFI_IF, self_mac);
    if (ret_mac != ESP_OK) {
        ESP_LOGE(TAG_ESPNOW_LOGIC, "TASK: Failed to get self MAC address: %s. Cannot ignore self-broadcasts reliably.", esp_err_to_name(ret_mac));
    } else {
        ESP_LOGI(TAG_ESPNOW_LOGIC, "TASK: Self MAC for ignoring broadcasts: " MACSTR, MAC2STR(self_mac));
    }

    ESP_LOGI(TAG_ESPNOW_LOGIC, "TASK: Broadcast Processing Task Started. Device Magic: 0x%08"PRIX32, device_magic_number);
    ESP_LOGI(TAG_ESPNOW_LOGIC, "TASK: Broadcasting every %d ms, Payload size: %d bytes",
             CONFIG_ESPNOW_SEND_DELAY, (int)sizeof(app_espnow_data_t));

    TickType_t last_send_time = xTaskGetTickCount();

    while (true) {
        if ((xTaskGetTickCount() - last_send_time) >= pdMS_TO_TICKS(CONFIG_ESPNOW_SEND_DELAY)) {
            char current_message_text[32];
            snprintf(current_message_text, sizeof(current_message_text), "Dev #%"PRIu32, message_counter++);
            app_espnow_prepare_broadcast_data(&send_data_buffer, current_message_text, device_magic_number);

            ESP_LOGI(TAG_ESPNOW_LOGIC, "TASK: Sending broadcast: Seq=%u, Msg='%s', CRC=0x%04X",
                     send_data_buffer.seq_num, send_data_buffer.simple_message, send_data_buffer.crc);

            if (esp_now_send(s_app_broadcast_mac, (uint8_t *)&send_data_buffer, sizeof(app_espnow_data_t)) != ESP_OK) {
                ESP_LOGE(TAG_ESPNOW_LOGIC, "TASK: Broadcast send API call error");
            }
            last_send_time = xTaskGetTickCount();
        }

        if (xQueueReceive(s_app_espnow_event_queue, &evt, pdMS_TO_TICKS(100)) == pdTRUE) {
            switch (evt.id) {
                case APP_ESPNOW_SEND_CB: {
                    app_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                    if (send_cb->status == ESP_NOW_SEND_SUCCESS) {
                        ESP_LOGI(TAG_ESPNOW_LOGIC, "TASK: Send CB: SUCCESS to "MACSTR, MAC2STR(send_cb->mac_addr));
                    } else {
                        ESP_LOGW(TAG_ESPNOW_LOGIC, "TASK: Send CB: FAIL to "MACSTR", Status: %d", MAC2STR(send_cb->mac_addr), send_cb->status);
                    }
                    break;
                }
                case APP_ESPNOW_RECV_CB: {
                    app_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
                    ESP_LOGD(TAG_ESPNOW_LOGIC, "TASK: Dequeued RECV_CB from "MACSTR", data_len: %d", MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

                    // Self-broadcast check (already present from previous version)
                    bool is_self = false;
                    if (ret_mac == ESP_OK && memcmp(self_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN) == 0) {
                        is_self = true;
                        ESP_LOGI(TAG_ESPNOW_LOGIC, "TASK: Processing self-broadcast packet (Seq: %u, Len: %d)",
                                 (recv_cb->data_len >= offsetof(app_espnow_data_t, seq_num) + sizeof(uint16_t)) ? ((app_espnow_data_t*)recv_cb->data)->seq_num : 0,
                                 recv_cb->data_len);
                    }

                    if (recv_cb->data_len == sizeof(app_espnow_data_t)) {
                        app_espnow_data_t *parsed_data = (app_espnow_data_t *)recv_cb->data;
                        uint16_t received_crc = parsed_data->crc;
                        parsed_data->crc = 0;
                        uint16_t calculated_crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)parsed_data, sizeof(app_espnow_data_t));

                        if (calculated_crc == received_crc) {
                            if (is_self) { // Use the is_self flag determined earlier
                                ESP_LOGI(TAG_ESPNOW_LOGIC, "TASK: Self-broadcast (Seq: %u, Msg: '%s') parsed successfully.",
                                         parsed_data->seq_num, parsed_data->simple_message);
                            } else {
                                ESP_LOGI(TAG_ESPNOW_LOGIC, "TASK: Parsed Broadcast from OTHER ("MACSTR"): Seq: %u, Magic: 0x%08"PRIX32", Msg: '%s'",
                                         MAC2STR(recv_cb->mac_addr), parsed_data->seq_num, parsed_data->magic, parsed_data->simple_message);
                                // TODO: Add your application logic here for messages from other Espressif devices
                            }
                        } else {
                            ESP_LOGW(TAG_ESPNOW_LOGIC, "TASK: CRC Mismatch from "MACSTR". Expected 0x%04X, Got 0x%04X. Len: %d",
                                     MAC2STR(recv_cb->mac_addr), calculated_crc, received_crc, recv_cb->data_len);
                        }
                    } else {
                         ESP_LOGW(TAG_ESPNOW_LOGIC, "TASK: Received packet from "MACSTR" with unexpected length %d (expected %d) after queueing.",
                                 MAC2STR(recv_cb->mac_addr), recv_cb->data_len, (int)sizeof(app_espnow_data_t));
                         ESP_LOG_BUFFER_HEXDUMP(TAG_ESPNOW_LOGIC, recv_cb->data, recv_cb->data_len, ESP_LOG_WARN);
                    }
                    free(recv_cb->data);
                    break;
                }
                default:
                    ESP_LOGE(TAG_ESPNOW_LOGIC, "TASK: Unknown event type: %d", evt.id);
                    break;
            }
        }
    }
    app_espnow_deinit_simplified();
    vTaskDelete(NULL);
}

static esp_err_t app_espnow_subsystem_init_simplified(void)
{
    s_app_espnow_event_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(app_espnow_event_t));
    if (s_app_espnow_event_queue == NULL) {
        ESP_LOGE(TAG_ESPNOW_LOGIC, "Create ESP-NOW event queue fail");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_ESPNOW_LOGIC, "Initializing ESP-NOW service...");
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(app_espnow_send_cb_handler_simplified));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(app_espnow_recv_cb_handler_simplified));

#if CONFIG_ESPNOW_ENABLE_POWER_SAVE
    ESP_LOGI(TAG_ESPNOW_LOGIC, "Enabling ESP-NOW Power Save Features");
    ESP_ERROR_CHECK(esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW));
    ESP_ERROR_CHECK(esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL));
#endif

    ESP_LOGI(TAG_ESPNOW_LOGIC, "Setting ESP-NOW PMK (Primary Master Key)...");
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    ESP_LOGI(TAG_ESPNOW_LOGIC, "Adding broadcast peer "MACSTR"...", MAC2STR(s_app_broadcast_mac));
    esp_now_peer_info_t peer_info = {0};
    peer_info.channel = CONFIG_ESPNOW_CHANNEL;
    peer_info.ifidx = APP_ESPNOW_WIFI_IF;
    peer_info.encrypt = false;
    memcpy(peer_info.peer_addr, s_app_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer_info));

    ESP_LOGI(TAG_ESPNOW_LOGIC, "ESP-NOW subsystem initialized successfully.");
    return ESP_OK;
}

static void app_espnow_deinit_simplified(void)
{
    if (s_app_espnow_event_queue) {
        vQueueDelete(s_app_espnow_event_queue);
        s_app_espnow_event_queue = NULL;
    }
    esp_now_deinit();
    ESP_LOGI(TAG_ESPNOW_LOGIC, "ESP-NOW Deinitialized.");
}

esp_err_t app_espnow_init_and_start_task(void)
{
    ESP_LOGI(TAG_ESPNOW_LOGIC, "Starting ESP-NOW initialization sequence...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG_ESPNOW_LOGIC, "NVS: Erasing and re-initializing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG_ESPNOW_LOGIC, "NVS Initialized.");

    app_wifi_init_simplified();

    if (app_espnow_subsystem_init_simplified() != ESP_OK) {
        ESP_LOGE(TAG_ESPNOW_LOGIC, "ESP-NOW subsystem initialization failed.");
        return ESP_FAIL;
    }

    if (xTaskCreate(app_espnow_broadcast_processing_task, "espnow_bcast_task", 3072 + 1024, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG_ESPNOW_LOGIC, "Failed to create app_espnow_broadcast_processing_task");
        app_espnow_deinit_simplified();
        return ESP_FAIL;
    }
    ESP_LOGI(TAG_ESPNOW_LOGIC, "app_espnow_broadcast_processing_task created.");
    return ESP_OK;
}
