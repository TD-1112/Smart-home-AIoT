#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdlib.h>
#include "lib_uart.h"
#include "lib_math.h"
#include "fsm.h"
#include "message.h"
#include "wifi_config.h"
#include "my_mqtt.h"
#include "cJSON.h"
#include "uart_protocol.h"
#include "define.h"
#include "help_function.h"

my_mqtt_init_t mqtt_cfg = {
    .server = "mqtt://broker.emqx.io", // Server broker
    .topic_pub = "GateWays/Server",    // topic publish
    .topic_sub = "Server/Gateways"     // topic subscribe
};

const char *UART_TAG = "UART_TASK";
const char *MQTT_TAG = "MQTT_TASK";
const char *MAIN_TAG = "MAIN";

Frame_Message mess;
QueueHandle_t json_queue;    // Queue to send JSON from UART task to MQTT task
QueueHandle_t mqtt_rx_queue; // Queue to receive MQTT messages

typedef struct
{
    char json_data[512];
    char topic[64];
} mqtt_message_t;

/**
 * @brief Callback when WiFi is connected
 * @details This function is called by the wifi_config component when the device gets an IP address.
 */
void wifi_connected_callback(void)
{
    ESP_LOGI(MAIN_TAG, "WiFi Connected Callback: Got IP.");
    // The state is already set to WIFI_STATE_CONNECTED inside wifi_config
}

/**
 * @brief Callback when MQTT is connected
 *
 */
void on_mqtt_connected(void)
{
    ESP_LOGI(MAIN_TAG, "MQTT connected!");
    // Update the system state to reflect MQTT is also connected
    wifi_config_set_state(WIFI_STATE_MQTT_CONNECTED);
}

/**
 * @brief TASK receive UART and decode message
 *@details This task continuously checks for incoming UART messages.
 * @param pvParameters
 */
void uart_receive_decode_task(void *pvParameters)
{
    ESP_LOGI(UART_TAG, "UART Receive & Decode Task Started\n");

    while (1)
    {
        if (uart.check.is_message())
        {
            decode_message(&mess);
            if (mess.type_message == UART_MSG_DATA && mess.length_message >= 5)
            {
                uint8_t flags = mess.data[0];
                uint16_t lux = (mess.data[1] << 8) | mess.data[2];
                uint8_t temp = mess.data[3];
                uint8_t humi = mess.data[4];

                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "type", "telemetry");
                cJSON *data = cJSON_CreateObject();
                if (flags & SENSOR_FLAG_LUX)
                    cJSON_AddNumberToObject(data, "lux", lux);
                if (flags & SENSOR_FLAG_TEMP)
                    cJSON_AddNumberToObject(data, "temp", temp);
                if (flags & SENSOR_FLAG_HUMI)
                    cJSON_AddNumberToObject(data, "humi", humi);
                cJSON_AddItemToObject(root, "data", data);

                char *json_str = cJSON_PrintUnformatted(root);
                cJSON_Delete(root);

                if (json_str != NULL)
                {
                    mqtt_message_t mqtt_msg;
                    strncpy(mqtt_msg.json_data, json_str, sizeof(mqtt_msg.json_data) - 1);
                    strncpy(mqtt_msg.topic, mqtt_cfg.topic_pub, sizeof(mqtt_msg.topic) - 1);
                    if (xQueueSend(json_queue, &mqtt_msg, pdMS_TO_TICKS(100)) != pdTRUE)
                    {
                        ESP_LOGW(UART_TAG, "JSON Queue full, message dropped");
                    }
                    free(json_str);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief TASK receive MQTT control messages and send UART commands
 * @details This task listens for control messages from MQTT and sends corresponding UART commands.
 * @param pvParameters
 */
void mqtt_receive_control_task(void *pvParameters)
{
    ESP_LOGI(MQTT_TAG, "MQTT Receive Control Task Started\n");
    my_mqtt_message_t mqtt_msg;
    uint8_t uart_buffer[256];

    while (1)
    {
        if (my_mqtt_getmess(&mqtt_msg))
        {
            ESP_LOGI(MQTT_TAG, "Received '%.*s' on topic '%.*s'", mqtt_msg.payload_len, mqtt_msg.payload, mqtt_msg.topic_len, mqtt_msg.topic);
            cJSON *root = cJSON_ParseWithLength(mqtt_msg.payload, mqtt_msg.payload_len);
            if (root)
            {
                cJSON *type = cJSON_GetObjectItem(root, "type");
                cJSON *data = cJSON_GetObjectItem(root, "data");
                if (type && cJSON_IsString(type) && strcmp(type->valuestring, "control") == 0 && data)
                {
                    cJSON *plug = cJSON_GetObjectItem(data, "plug");
                    cJSON *status = cJSON_GetObjectItem(data, "status");
                    if (plug && cJSON_IsString(plug) && status && cJSON_IsString(status))
                    {
                        int plug_id = 0;
                        if (sscanf(plug->valuestring, "plug_%d", &plug_id) == 1)
                        {
                            Plug_Status a_status = (strcasecmp(status->valuestring, "on") == 0) ? STATUS_ON : STATUS_OFF;
                            uint16_t length = create_uart_control_message(plug_id - 1, a_status, uart_buffer);
                            if (length > 0)
                            {
                                uart.send.bytes(uart_buffer, length);
                                ESP_LOGI(MQTT_TAG, "Sent UART control for Plug %d to %s", plug_id, status->valuestring);
                            }
                        }
                    }
                }
                cJSON_Delete(root);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief TASK publish JSON messages to MQTT
 * @details This task waits for JSON messages from the UART task and publishes them to MQTT.
 *
 * @param pvParameters
 */
void mqtt_publish_task(void *pvParameters)
{
    ESP_LOGI(MQTT_TAG, "MQTT Publish Task Started\n");
    mqtt_message_t mqtt_msg;

    while (1)
    {
        if (xQueueReceive(json_queue, &mqtt_msg, portMAX_DELAY) == pdTRUE)
        {
            if (wifi_config_get_state() == WIFI_STATE_MQTT_CONNECTED)
            {
                ESP_LOGI(MQTT_TAG, "Publishing to %s: %s", mqtt_msg.topic, mqtt_msg.json_data);
                my_mqtt_pub(mqtt_msg.topic, mqtt_msg.json_data);
            }
            else
            {
                ESP_LOGW(MQTT_TAG, "Not connected to MQTT, dropping message.");
            }
        }
    }
}

void app_main(void)
{
    uart_init_with_fsm(UART_BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
    ESP_LOGI(MAIN_TAG, "UART initialized.");

    // Start the WiFi manager
    ESP_LOGI(MAIN_TAG, "Starting WiFi Manager...");
    wifi_config_start(wifi_connected_callback);

    // Start LED status task AFTER wifi_config_start to ensure mutex is created
    xTaskCreate(led_status_task, "led_status", 2048, NULL, 2, NULL);

    // Wait until WiFi is connected or AP mode is active
    ESP_LOGI(MAIN_TAG, "Waiting for WiFi connection or AP mode...");
    while (wifi_config_get_state() != WIFI_STATE_CONNECTED && wifi_config_get_state() != WIFI_STATE_AP_MODE)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // If we are in AP mode, we don't proceed to connect MQTT.
    if (wifi_config_get_state() == WIFI_STATE_AP_MODE)
    {
        ESP_LOGI(MAIN_TAG, "Device is in AP Mode. Halting main task.");
        // The device will restart when configured via web page.
        vTaskDelete(NULL); // Stop further execution in main.
        return;
    }

    ESP_LOGI(MAIN_TAG, "WiFi is ready! Proceeding with MQTT connection.");

    // Connect to MQTT
    ESP_LOGI(MAIN_TAG, "Connecting to MQTT broker: %s", mqtt_cfg.server);
    // Link the on_mqtt_connected callback to the mqtt component
    mqtt_cfg.on_connected_cb = on_mqtt_connected;
    esp_err_t err_mqtt = my_mqtt_init(&mqtt_cfg);
    if (err_mqtt != ESP_OK)
    {
        ESP_LOGE(MAIN_TAG, "MQTT init failed!");
        wifi_config_set_state(WIFI_STATE_ERROR);
    }

    // Wait for MQTT to be connected. The state will be set in the callback.
    int mqtt_retry = 0;
    while (wifi_config_get_state() != WIFI_STATE_MQTT_CONNECTED && mqtt_retry < 60)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        mqtt_retry++;
    }

    if (wifi_config_get_state() == WIFI_STATE_MQTT_CONNECTED)
    {
        ESP_LOGI(MAIN_TAG, "System is fully operational (WiFi + MQTT).");
    }
    else
    {
        ESP_LOGW(MAIN_TAG, "MQTT connection timeout! LED will remain off.");
        // The LED task will handle the visual state
    }

    // Create queues
    json_queue = xQueueCreate(10, sizeof(mqtt_message_t));
    if (!json_queue)
    {
        ESP_LOGE(MAIN_TAG, "Failed to create JSON queue!");
        return;
    }

    ESP_LOGI(MAIN_TAG, "Starting application tasks...");
    xTaskCreate(uart_receive_decode_task, "uart_rx_decode", 4096, NULL, 5, NULL);
    xTaskCreate(mqtt_publish_task, "mqtt_publish", 4096, NULL, 4, NULL);
    xTaskCreate(mqtt_receive_control_task, "mqtt_rx_control", 4096, NULL, 4, NULL);

    ESP_LOGI(MAIN_TAG, "System setup complete.");
}
