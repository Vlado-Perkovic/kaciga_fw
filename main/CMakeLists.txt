idf_component_register(SRCS
                           "main.c" "sensor.c"
                           "display_logic.c"
                           "sensor_logic.c"
                           "espnow_logic.c"
                    # PRIV_REQUIRES nvs_flash esp_wifi esp_event esp_netif
                    REQUIRES driver SSD1306_Driver nvs_flash esp_wifi esp_event esp_netif
                    INCLUDE_DIRS ".")
