#include <stdio.h>
#include "touch.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lcd.h"
#include "oscil_pins_display.h"

static const char *TAG = "touch";
static i2c_master_bus_handle_t s_i2c;
static bool s_ok;

// Every 7-bit address; prints the ones that ACK
static void i2c_scan(void)
{
    int n = 0;
    for (uint16_t a = 0x08; a < 0x78; a++)
        if (i2c_master_probe(s_i2c, a, 20) == ESP_OK) { printf("i2c: found 0x%02X\n", a); n++; }
    printf("i2c: %d device(s)\n", n);
}

esp_err_t touch_start(lv_display_t *disp)
{
    const i2c_master_bus_config_t bus = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = DISP_TOUCH_SDA, .scl_io_num = DISP_TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,          // 4.7k to +3V3_DISP on the board
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus, &s_i2c), TAG, "i2c bus");

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();   // 0x5D, 16-bit registers
    io_cfg.scl_speed_hz = 400000;                       // GT911 p.10: at or below 400 kbit/s
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c, &io_cfg, &io), TAG, "panel io");

    // With driver_data the driver runs the address-select sequence: RESET low, INT low
    // (0x5D; high would pick 0x14), RESET high, wait, INT released to input
    // (GT911 p.11, programming guide p.24). Without it INT floats during reset.
    static esp_lcd_touch_io_gt911_config_t gt = { .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS };
    const esp_lcd_touch_config_t tcfg = {
        .x_max = LCD_H_RES, .y_max = LCD_V_RES,
        .rst_gpio_num = DISP_TOUCH_RESET, .int_gpio_num = DISP_TOUCH_INT,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },   // record what the corners need
        .driver_data = &gt,
    };
    esp_lcd_touch_handle_t tp;
    esp_err_t e = esp_lcd_touch_new_i2c_gt911(io, &tcfg, &tp);
    i2c_scan();                                         // after the driver released RESET
    ESP_RETURN_ON_ERROR(e, TAG, "GT911 not answering");

    const lvgl_port_touch_cfg_t lt = { .disp = disp, .handle = tp };
    lvgl_port_lock(0);
    lv_indev_t *indev = lvgl_port_add_touch(&lt);
    lvgl_port_unlock();
    s_ok = (indev != NULL);
    return s_ok ? ESP_OK : ESP_FAIL;
}

bool touch_ok(void)
{
    return s_ok;
}