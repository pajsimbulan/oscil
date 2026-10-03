#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define NET_ONLINE BIT0                   // set while the station has an IP address

extern EventGroupHandle_t g_net_bits;     // other tasks wait on NET_ONLINE, nobody polls

esp_err_t net_start(void);                // needs NVS (settings_init) first
bool      net_online(void);
int       net_rssi(void);                 // dBm, 0 when offline