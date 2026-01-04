idf_component_register(SRCS "my_wifi.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_event nvs_flash esp_netif
                    )