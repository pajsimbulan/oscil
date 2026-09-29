#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "lcd.h"
#include "lcd_test.h"

// One line of pixels, drawn once per row. draw_bitmap copies it into the PSRAM frame
// buffer and writes the cache back, so the DMA never reads stale pixels.
static void fill_rect(esp_lcd_panel_handle_t p, int x0, int y0, int x1, int y1, uint16_t c)
{
    static uint16_t line[LCD_H_RES];
    for (int x = 0; x < x1 - x0; x++) line[x] = c;
    for (int y = y0; y < y1; y++) esp_lcd_panel_draw_bitmap(p, x0, y, x1, y + 1, line);
}

static void pattern_bars(esp_lcd_panel_handle_t p)
{
    // white, yellow, cyan, green, magenta, red, blue, black
    static const uint16_t C[8] = { 0xFFFF, 0xFFE0, 0x07FF, 0x07E0, 0xF81F, 0xF800, 0x001F, 0x0000 };
    for (int b = 0; b < 8; b++) fill_rect(p, b * 100, 0, b * 100 + 100, LCD_V_RES, C[b]);
}

static void pattern_bits(esp_lcd_panel_handle_t p)
{
    // 16 bands. Band b drives only data line b high: an open wire is a black band.
    for (int b = 0; b < 16; b++) fill_rect(p, b * 50, 0, b * 50 + 50, LCD_V_RES, (uint16_t)(1u << b));
}

static void pattern_border(esp_lcd_panel_handle_t p)
{
    fill_rect(p, 0, 0, LCD_H_RES, LCD_V_RES, 0x0000);
    fill_rect(p, 0, 0, LCD_H_RES, 1, 0xFFFF);
    fill_rect(p, 0, LCD_V_RES - 1, LCD_H_RES, LCD_V_RES, 0xFFFF);
    fill_rect(p, 0, 0, 1, LCD_V_RES, 0xFFFF);
    fill_rect(p, LCD_H_RES - 1, 0, LCD_H_RES, LCD_V_RES, 0xFFFF);
}

void test_lcd(esp_lcd_panel_handle_t panel)
{
    for (;;) {
        printf("lcd: bars\n");   pattern_bars(panel);   vTaskDelay(pdMS_TO_TICKS(5000));
        printf("lcd: bits\n");   pattern_bits(panel);   vTaskDelay(pdMS_TO_TICKS(5000));
        printf("lcd: border\n"); pattern_border(panel); vTaskDelay(pdMS_TO_TICKS(5000));
    }
}