idf_component_register(
    SRCS "main.c" "lcd.c" "lcd_test.c" "gui.c" "gui_test.c" "touch.c" "touch_test.c"
    INCLUDE_DIRS "."
    REQUIRES shared esp_lcd esp_driver_i2c esp_driver_gpio esp_psram spi_flash)