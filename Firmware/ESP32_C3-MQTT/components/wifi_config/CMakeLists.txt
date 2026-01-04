idf_component_register(SRCS "wifi_config.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_http_server esp_wifi driver)
