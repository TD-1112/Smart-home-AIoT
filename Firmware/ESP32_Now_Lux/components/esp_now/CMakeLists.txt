idf_component_register(
    SRCS "my_espnow.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_now nvs_flash
)
