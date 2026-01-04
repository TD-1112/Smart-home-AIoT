#ifndef __MY_WIFI_H__
#define __MY_WIFI_H__

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define MAX_RETRY 5

esp_err_t wifi_init_sta(const char *ssid, const char *pass, uint8_t channel);
esp_err_t wifi_init_for_esp_now(void);

#endif