file(GLOB_RECURSE app_sources 
    "Src/*.c"
    "Src/*.cpp" 
    "Src/*.cc"
)

idf_component_register(SRCS ${app_sources}
                    INCLUDE_DIRS "Include"
                    REQUIRES my_wifi esp_now cjson my_mqtt lib_uart fsm message lib_math)
