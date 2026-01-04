#include "help_function.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "wifi_config.h"
#include "define.h" // For LED_PIN

void led_status_task(void *pvParameters)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);

    uint8_t led_level = 0;
    wifi_manager_state_t last_state = WIFI_STATE_UNINITIALIZED;
    while (1)
    {
        wifi_manager_state_t current_state = wifi_config_get_state();
        if (current_state != last_state) {
            ESP_LOGI("LED_TASK", "State changed to %d", current_state);
            last_state = current_state;
        }

        switch (current_state)
        {
        case WIFI_STATE_MQTT_CONNECTED:
            // Solid ON when fully connected
            led_level = 1;
            gpio_set_level(LED_PIN, led_level);
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;

        case WIFI_STATE_AP_MODE:
            // Fast blink in AP mode
            led_level = !led_level;
            gpio_set_level(LED_PIN, led_level);
            vTaskDelay(pdMS_TO_TICKS(250));
            break;

        case WIFI_STATE_CONNECTING:
        case WIFI_STATE_CONNECTED: // Still OFF until MQTT is connected
        case WIFI_STATE_UNINITIALIZED:
        case WIFI_STATE_ERROR:
        default:
            // LED OFF during connection attempts or on failure
            led_level = 0;
            gpio_set_level(LED_PIN, led_level);
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        }
    }
}
