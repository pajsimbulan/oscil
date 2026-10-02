idf_component_register(
    SRCS "main.c" "lcd.c" "lcd_test.c" "gui.c" "gui_test.c" "touch.c" "touch_test.c"
         "links.c" "scope_view.c" "ui.c" "ui_gen.c"
    INCLUDE_DIRS "."
    REQUIRES shared esp_lcd esp_driver_i2c esp_driver_gpio esp_timer esp_psram spi_flash)