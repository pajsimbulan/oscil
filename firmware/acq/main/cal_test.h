#pragma once

// Two-point calibration of one channel (0 = CH1). Run with the acquisition task NOT started:
// it drives GP-SPI2 directly. Returns after storing the result in NVS.
void test_cal(int ch);