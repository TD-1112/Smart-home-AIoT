/**
 * @file main.c
 * @brief Main application - Speech Commands Recognition
 * @details Initialize all modules and create tasks
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_config.h"
#include "plug_control.h"
#include "audio_feed.h"
#include "speech_detect.h"
#include "my_wifi.h"

static const char *TAG = "[MAIN]";

void app_main(void)
{
    // ========= WiFi Init ==========
    ESP_LOGI(TAG, "Initializing WiFi...");
    esp_err_t err_wifi = wifi_init_sta(WIFI_SSID, WIFI_PASSWORD);
    if (err_wifi != ESP_OK)
    {
        ESP_LOGE(TAG, "WiFi init failed");
    }
    else
    {
        ESP_LOGI(TAG, "WiFi init OK");
    }

    // ========== Plug Control & MQTT Init ==========
    ESP_LOGI(TAG, "Initializing Plug Control & MQTT...");
    esp_err_t err_plug = plug_control_init();
    if (err_plug != ESP_OK)
    {
        ESP_LOGE(TAG, "Plug control init failed");
        return;
    }

    // ========== Speech Models Init ==========
    ESP_LOGI(TAG, "Initializing Speech Models...");
    esp_err_t err_models = speech_models_init();
    if (err_models != ESP_OK)
    {
        ESP_LOGE(TAG, "Speech models init failed");
        return;
    }

    // ========== I2S Init ==========
    ESP_LOGI(TAG, "Initializing I2S...");
    esp_err_t err_i2s = audio_i2s_init();
    if (err_i2s != ESP_OK)
    {
        ESP_LOGE(TAG, "I2S init failed");
        return;
    }

    // ========== AFE Init ==========
    ESP_LOGI(TAG, "Initializing AFE...");
    esp_err_t err_afe = speech_afe_init();
    if (err_afe != ESP_OK)
    {
        ESP_LOGE(TAG, "AFE init failed");
        return;
    }

    // ========== Create Tasks ==========
    ESP_LOGI(TAG, "Creating tasks...");

    // Task 1: Feed Task - Read audio from I2S and feed to AFE
    xTaskCreatePinnedToCore(
        feed_task,
        "feed_task",
        FEED_TASK_STACK_SIZE,
        NULL,
        FEED_TASK_PRIORITY,
        NULL,
        FEED_TASK_CORE);
    ESP_LOGI(TAG, "  [OK] feed_task on Core %d", FEED_TASK_CORE);

    // Task 2: Detect Task - Process WakeNet + MultiNet
    xTaskCreatePinnedToCore(
        detect_task,
        "detect_task",
        DETECT_TASK_STACK_SIZE,
        NULL,
        DETECT_TASK_PRIORITY,
        NULL,
        DETECT_TASK_CORE);
    ESP_LOGI(TAG, "  [OK] detect_task on Core %d", DETECT_TASK_CORE);

    // Task 3: MQTT Task - Handle MQTT message publishing
    xTaskCreatePinnedToCore(
        mqtt_task,
        "mqtt_task",
        MQTT_TASK_STACK_SIZE,
        NULL,
        MQTT_TASK_PRIORITY,
        NULL,
        MQTT_TASK_CORE);
    ESP_LOGI(TAG, "  [OK] mqtt_task on Core %d", MQTT_TASK_CORE);
}
