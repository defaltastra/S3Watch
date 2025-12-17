idf_component_register(
    SRCS "power_button.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos esp_common
)