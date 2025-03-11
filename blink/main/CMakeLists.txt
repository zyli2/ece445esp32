idf_component_register(
    SRCS
        "blink_example_main.c"
        "gap.c"
    INCLUDE_DIRS "."
    REQUIRES
        bt
        esp_driver_gpio
        nvs_flash
)