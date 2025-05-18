#ifndef ESPNOW_COMMON_H
#define ESPNOW_COMMON_H

#include "esp_now.h" // For esp_now_peer_info_t

// Define the structure of the message to be sent
// Keep it simple and ensure its size is less than ESP_NOW_MAX_DATA_LEN (250 bytes)
typedef struct {
    int message_id;
    char message_content[32]; // A simple text message
    uint32_t crc; // Optional: for basic data integrity check
} espnow_message_t;

// ESP-NOW broadcast MAC address
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Optional: A simple CRC32 calculation function (example)
// In a real application, you might use a more robust CRC or a library function.
static uint32_t calculate_crc32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    while (length--) {
        crc ^= (uint32_t)(*data++) << 24;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ 0x04C11DB7; // CRC32 polynomial
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

#endif // ESPNOW_COMMON_H

