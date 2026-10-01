idf_component_register(
    SRCS "main.c" "lcd.c" "lcd_test.c" "gui.c" "gui_test.c"
    INCLUDE_DIRS "."
    REQUIRES shared esp_lcd esp_psram spi_flash)