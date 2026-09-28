#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "oscil_pins_acq.h"
#include "oscil_status.h"
#include "oscil_pinwalk.h"
#include "spi_speed.h"
#include "panel.h"
#include "panel_test.h"        
#include "adc_test.h"
#include "burst_test.h"
#include "testsig.h"
#include "acq_task.h"
#include "link_test.h"

static const char *TAG = "acq";

/**
 * 
 static const oscil_pin_t ACQ_PINS[] = {
    { ACQ_ENC1_A, "ENC1_A" }, { ACQ_ENC1_B, "ENC1_B" }, { ACQ_ENC1_SW, "ENC1_SW" },
    { ACQ_ENC2_A, "ENC2_A" }, { ACQ_ENC2_B, "ENC2_B" }, { ACQ_ENC2_SW, "ENC2_SW" },
    { ACQ_ADC_SCLK, "ADC_SCLK" }, { ACQ_ADC_CS, "ADC_CS" }, { ACQ_CH2_SDO, "CH2_SDO" },
    { ACQ_ENC3_A, "ENC3_A" },
    { ACQ_CH1_SDO, "CH1_SDO" }, { ACQ_ENC3_B, "ENC3_B" }, { ACQ_ENC3_SW, "ENC3_SW" },
    { ACQ_BTN_RUN, "BTN_RUN" }, { ACQ_BTN_SINGLE, "BTN_SINGLE" }, { ACQ_BTN_GEN, "BTN_GEN" },
    { ACQ_LED_RUN, "LED_RUN" }, { ACQ_LED_TRIG, "LED_TRIG" }, { ACQ_LED_ARM, "LED_ARM" },
    { ACQ_LINK1_TX, "LINK1_TX" }, { ACQ_LINK1_RX, "LINK1_RX" },
    { 9, "spare" }, { 14, "spare" }, { 21, "spare" }, { 38, "spare" }, { 47, "spare" },
};

*/

void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    uint32_t flash_bytes = 0;
    esp_flash_get_size(NULL, &flash_bytes);

    ESP_LOGI(TAG, "Oscil board 1: acquisition");
    ESP_LOGI(TAG, "cores %d, silicon v%d.%d",
             chip.cores, chip.revision / 100, chip.revision % 100);
    ESP_LOGI(TAG, "flash %" PRIu32 " MB", flash_bytes / (1024 * 1024));
    ESP_LOGI(TAG, "psram %u MB", (unsigned)(esp_psram_get_size() / (1024 * 1024)));
    ESP_LOGI(TAG, "tick %d Hz, LINK1 on GPIO%d/%d",
             configTICK_RATE_HZ, ACQ_LINK1_TX, ACQ_LINK1_RX);
    ESP_ERROR_CHECK(oscil_status_init(ACQ_STATUS_RGB));
    ESP_ERROR_CHECK(oscil_status_start_heartbeat(0,16,0)); //green
    ESP_LOGI(TAG,"heartbeat on GPIO%d", ACQ_STATUS_RGB);
    //test_pinwalk(ACQ_PINS, sizeof ACQ_PINS / sizeof ACQ_PINS[0]);
    //test_spi_speed();
    // test_panel();
    //test_adc();
    //test_noise();
    //test_dual();
    //test_burst();
    testsig_start(47, 1000);        // bench signal on header 28 until board 3 exists
    //ESP_ERROR_CHECK(acq_start());
    test_link_loop();
}