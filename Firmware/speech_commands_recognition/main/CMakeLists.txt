file(GLOB_RECURSE app_sources 
    "Src/*.c"
    "Src/*.cpp" 
    "Src/*.cc"
)
set(requires
    esp-sr
    led_strip
    hardware_driver
    cjson
    my_wifi
    my_mqtt
    )

idf_component_register(SRCS ${app_sources}
                       INCLUDE_DIRS include
                       REQUIRES ${requires})

component_compile_options(-w)