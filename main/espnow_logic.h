/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifndef ESPNOW_LOGIC_H // Renamed from ESPNOW_EXAMPLE_H
#define ESPNOW_LOGIC_H

#include "esp_now.h"    // For esp_now_peer_info_t, ESP_NOW_ETH_ALEN etc.
#include "esp_wifi.h"   // For WIFI_MODE_STA, WIFI_MODE_AP etc.

/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */
/* For simplicity in this integration, we'll default to STA mode if not configured. */
#ifndef CONFIG_ESPNOW_WIFI_MODE_STATION
#define CONFIG_ESPNOW_WIFI_MODE_STATION 1 // Default to STA mode
#endif

#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define APP_ESPNOW_WIFI_MODE WIFI_MODE_STA
#define APP_ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
#else
#define APP_ESPNOW_WIFI_MODE WIFI_MODE_AP
#define APP_ESPNOW_WIFI_IF   ESP_IF_WIFI_AP
#endif

// Default Kconfig values if not set by user (for a self-contained example)
#ifndef CONFIG_ESPNOW_CHANNEL
#define CONFIG_ESPNOW_CHANNEL 1
#endif
#ifndef CONFIG_ESPNOW_SEND_LEN
#define CONFIG_ESPNOW_SEND_LEN 100 // A reasonable default payload length
#endif
#ifndef CONFIG_ESPNOW_PMK
#define CONFIG_ESPNOW_PMK "pmk1234567890123" // Default PMK (16 chars)
#endif
#ifndef CONFIG_ESPNOW_LMK
#define CONFIG_ESPNOW_LMK "lmk1234567890123" // Default LMK (16 chars)
#endif
#ifndef CONFIG_ESPNOW_SEND_COUNT
#define CONFIG_ESPNOW_SEND_COUNT 100 // Example send count
#endif
#ifndef CONFIG_ESPNOW_SEND_DELAY
#define CONFIG_ESPNOW_SEND_DELAY 1000 // Example send delay (ms)
#endif


#define ESPNOW_QUEUE_SIZE           6
#define ESPNOW_MAXDELAY             512 // Milliseconds for queue operations

// External declaration for the broadcast MAC, defined in espnow_logic.c
extern uint8_t s_app_broadcast_mac[ESP_NOW_ETH_ALEN];

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_app_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

typedef enum {
    APP_ESPNOW_SEND_CB,
    APP_ESPNOW_RECV_CB,
} app_espnow_event_id_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} app_espnow_event_send_cb_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} app_espnow_event_recv_cb_t;

typedef union {
    app_espnow_event_send_cb_t send_cb;
    app_espnow_event_recv_cb_t recv_cb;
} app_espnow_event_info_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task. */
typedef struct {
    app_espnow_event_id_t id;
    app_espnow_event_info_t info;
} app_espnow_event_t;

enum {
    APP_ESPNOW_DATA_BROADCAST,
    APP_ESPNOW_DATA_UNICAST,
    APP_ESPNOW_DATA_MAX,
};

/* User defined field of ESPNOW data in this example. */
typedef struct {
    uint8_t type;             // Broadcast or unicast ESPNOW data.
    uint8_t state;            // Indicate that if has received broadcast ESPNOW data or not.
    uint16_t seq_num;         // Sequence number of ESPNOW data.
    uint16_t crc;             // CRC16 value of ESPNOW data.
    uint32_t magic;           // Magic number which is used to determine which device to send unicast ESPNOW data.
    char simple_message[32];  // Added for a simple text message
    // uint8_t payload[0];    // Real payload of ESPNOW data. // We'll use a fixed struct for simplicity
} __attribute__((packed)) app_espnow_data_t;

/* Parameters of sending ESPNOW data. */
typedef struct {
    bool unicast;                       // Send unicast ESPNOW data.
    bool broadcast;                     // Send broadcast ESPNOW data.
    uint8_t state;                      // Indicate that if has received broadcast ESPNOW data or not.
    uint32_t magic;                     // Magic number.
    uint16_t count;                     // Total count of unicast ESPNOW data to be sent (not used for continuous broadcast).
    uint16_t delay;                     // Delay between sending two ESPNOW data, unit: ms.
    int len;                            // Length of ESPNOW data to be sent, unit: byte.
    uint8_t *buffer;                    // Buffer pointing to ESPNOW data.
    uint8_t dest_mac[ESP_NOW_ETH_ALEN]; // MAC address of destination device.
} app_espnow_send_param_t;


// Public function to initialize ESP-NOW
esp_err_t app_espnow_init_and_start_task(void);

#endif // ESPNOW_LOGIC_H
