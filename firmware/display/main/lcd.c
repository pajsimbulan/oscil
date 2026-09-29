#include "lcd.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "oscil_pins_display.h"

static const char *TAG = "lcd";
static bool s_ok;

// Every number below is from ORIENT p.11 (typical column) unless marked.
#define LCD_PCLK_HZ  (16 * 1000 * 1000)  // start low. Panel max 50 MHz (ORIENT p.11),
                                         // S3 LCD interface max 40 MHz (ESP32-S3 datasheet p.52)
#define HSYNC_PW     48                  // thpw
#define HSYNC_BP     (88 - 48)           // thb 88 includes the pulse: 800 + 88 + 40 = 928 = th
#define HSYNC_FP     40                  // thfp
#define VSYNC_PW     3                   // tvpw
#define VSYNC_BP     (32 - 3)            // tvb 32 includes the pulse: 480 + 32 + 13 = 525 = tv
#define VSYNC_FP     13                  // tvfp

esp_lcd_panel_handle_t lcd_start(int bounce_lines, int num_fbs)
{
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = LCD_PCLK_HZ,
            .h_res = LCD_H_RES, .v_res = LCD_V_RES,
            .hsync_pulse_width = HSYNC_PW, .hsync_back_porch = HSYNC_BP, .hsync_front_porch = HSYNC_FP,
            .vsync_pulse_width = VSYNC_PW, .vsync_back_porch = VSYNC_BP, .vsync_front_porch = VSYNC_FP,
            // HSYNC/VSYNC idle high, pulse low: "Negative polarity", ORIENT p.9 (the default).
            // The sampling edge isn't in the text; if the image shimmers, try 0.
            .flags = { .pclk_active_neg = 1 },
        },
        .data_width = 16,                               // RGB565
        .bits_per_pixel = 16,
        .num_fbs = num_fbs,
        .bounce_buffer_size_px = bounce_lines * LCD_H_RES,
        .dma_burst_size = 64,                           // IDF 5.4+ name (older: psram_trans_align)
        .hsync_gpio_num = DISP_LCD_HSYNC, .vsync_gpio_num = DISP_LCD_VSYNC,
        .de_gpio_num = DISP_LCD_DE, .pclk_gpio_num = DISP_LCD_DCLK,
        .disp_gpio_num = -1,                            // DISP tied high through 10k
        .data_gpio_nums = {                             // D0..D15: B3..B7, G2..G7, R3..R7
            DISP_LCD_B3, DISP_LCD_B4, DISP_LCD_B5, DISP_LCD_B6, DISP_LCD_B7,
            DISP_LCD_G2, DISP_LCD_G3, DISP_LCD_G4, DISP_LCD_G5, DISP_LCD_G6, DISP_LCD_G7,
            DISP_LCD_R3, DISP_LCD_R4, DISP_LCD_R5, DISP_LCD_R6, DISP_LCD_R7,
        },
        .flags = { .fb_in_psram = 1 },                  // 768 KB per frame: PSRAM only
    };
    esp_lcd_panel_handle_t p = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &p));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(p));
    ESP_ERROR_CHECK(esp_lcd_panel_init(p));
    s_ok = true;

    const int h_total = LCD_H_RES + HSYNC_PW + HSYNC_BP + HSYNC_FP;   // 928
    const int v_total = LCD_V_RES + VSYNC_PW + VSYNC_BP + VSYNC_FP;   // 525
    ESP_LOGI(TAG, "%dx%d, pclk %d MHz, %.1f Hz refresh, bounce %d lines, %d fb",
             LCD_H_RES, LCD_V_RES, LCD_PCLK_HZ / 1000000,
             (double)LCD_PCLK_HZ / (h_total * v_total), bounce_lines, num_fbs);
    return p;
}

bool lcd_ok(void)
{
    return s_ok;
}