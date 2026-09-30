idf_component_register(
    SRCS "gen_test.c" "gen.c" "main.c" "r2r.c" "r2r_test.c"
    INCLUDE_DIRS "."
    REQUIRES shared esp_psram spi_flash nvs_flash
)