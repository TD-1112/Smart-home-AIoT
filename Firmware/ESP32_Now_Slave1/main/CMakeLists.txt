idf_component_register(SRCS "main.c" "api.c"
                    INCLUDE_DIRS "."
                    REQUIRES my_wifi esp_now lib_uart lib_math fsm message)
