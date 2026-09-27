idf_component_register(
    SRCS "adc_test.c" "panel_test.c" "panel.c" "main.c" "spi2_adc.c" "spi_speed.c"
    INCLUDE_DIRS "."
    REQUIRES shared esp_psram spi_flash
)