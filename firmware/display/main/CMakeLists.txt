idf_component_register(
    SRCS "ui_photos.c" "upload.c" "ui_account.c" "account_test.c" "account.c" "cloud_http.c" "main.c" "lcd.c" "lcd_test.c" "gui.c" "gui_test.c" "touch.c" "touch_test.c"
         "links.c" "scope_view.c" "ui.c" "ui_gen.c" "settings.c" "net.c"
         "ota.c" "ui_sys.c"
    INCLUDE_DIRS "."
    REQUIRES shared esp_lcd esp_driver_i2c esp_driver_gpio esp_timer esp_psram spi_flash nvs_flash esp_rom
             esp_wifi esp_netif esp_event esp_http_client esp_https_ota app_update esp_app_format mbedtls json)