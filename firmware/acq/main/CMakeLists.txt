idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES shared esp_psram spi_flash
)