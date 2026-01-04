idf_component_register(
    SRCS "lib_uart.c"
    INCLUDE_DIRS "."
    REQUIRES "driver" "esp_common" fsm message
)