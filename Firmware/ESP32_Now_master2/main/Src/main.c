#include "esp_log.h"
#include "api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Json_message.h"
#include "my_wifi.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "uart_protocol.h"
#include "lib_uart.h"
#include "define.h"
#include "helper_function.h"

// ----- global variables -----
const char *Master_Tag = "MASTER";
const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t MASTER_MAC[6] = {0};

discovered_slave_t discovered_slaves[MAX_SLAVES];
volatile int slave_count = 0;
QueueHandle_t uart_json_queue = NULL;

// ----- Task prototypes -----
static void uart_bridge_task(void *pvParameters);
static void master_discovery_task(void *pvParameters);
static void espnow_receive_task(void *pvParameters);
static void data_request_task(void *pvParameters);


/**
 * @brief task uart bridge
 * @details This task bridges ESP-NOW JSON messages to UART frames. It waits for a cycle marker, then collects JSON messages within a defined time window,
 *          aggregates the sensor data, and sends a single UART frame with the aggregated data at the end of the window.
 * @param pvParameters
 */
static void uart_bridge_task(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(Master_Tag, "uart_bridge_task started");

    const TickType_t COLLECT_WINDOW = pdMS_TO_TICKS(200);
    uart_json_msg_t in = {0};
    uint8_t uart_frame[32];

    while (1)
    {
        // Wait for start-of-cycle signal
        if (xQueueReceive(uart_json_queue, &in, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        if (in.evt != UART_BRIDGE_EVT_CYCLE)
        {
            // Ignore stray JSON until we get a cycle marker
            continue;
        }

        Sensor_Data pending = {
            .flags = SENSOR_FLAG_NONE,
            .lux = 0,
            .temp = 0,
            .humi = 0,
        };

        TickType_t deadline = xTaskGetTickCount() + COLLECT_WINDOW;

        // Collect response_data JSONs within the window
        while (1)
        {
            TickType_t now = xTaskGetTickCount();
            if (now >= deadline)
            {
                break;
            }

            TickType_t wait = deadline - now;
            if (xQueueReceive(uart_json_queue, &in, wait) == pdTRUE)
            {
                if (in.evt == UART_BRIDGE_EVT_JSON)
                {
                    extract_sensor_data_from_json(in.json, &pending);
                }
                else if (in.evt == UART_BRIDGE_EVT_CYCLE)
                {
                    // New cycle arrived before we sent previous one: break and send immediately.
                    // Then this cycle marker will be processed next loop iteration.
                    // Put it back to queue front is not supported, so we just treat it as next cycle by sending now
                    // and starting a new window immediately.
                    // Start next cycle by resetting deadline and pending.
                    uint16_t frame_len = create_uart_data_message(&pending, uart_frame);
                    if (frame_len > 0)
                    {
                        uart.send.bytes(uart_frame, frame_len);
                        ESP_LOGI(Master_Tag, "UART sent sensor frame (len=%u)", (unsigned)frame_len);
                    }

                    pending.flags = SENSOR_FLAG_NONE;
                    pending.lux = 0;
                    pending.temp = 0;
                    pending.humi = 0;
                    deadline = xTaskGetTickCount() + COLLECT_WINDOW;
                }
            }
            else
            {
                break;
            }
        }

        // Send once per cycle, even if pending.flags == NONE (all default 0)
        uint16_t frame_len = create_uart_data_message(&pending, uart_frame);
        if (frame_len > 0)
        {
            uart.send.bytes(uart_frame, frame_len);
            ESP_LOGI(Master_Tag, "UART sent sensor frame (len=%u)", (unsigned)frame_len);
        }
        else
        {
            ESP_LOGW(Master_Tag, "Failed to create UART frame from aggregated data");
        }
    }
}


/**
 * @brief task send discovery message by scanning wifi channels
 * @details This task periodically sends a discovery message to all slaves every DISCOVERY_PERIOD_MS milliseconds if no slaves are currently connected.
 *          It scans through WiFi channels from WIFI_CHANNEL_MIN to WIFI_CHANNEL_MAX.
 * @param pvParameters
 */
static void master_discovery_task(void *pvParameters)
{
    ESP_LOGI(Master_Tag, "master_discovery_task started");
    json_master_msg_t discovery_msg = {
        .type = JSON_MSG_TYPE_DISCOVERY,
        .has_cmd = false};
    mac_to_string(MASTER_MAC, discovery_msg.id);
    strcpy(discovery_msg.dst, "broadcast");

    char *json_msg = json_encode_master_msg(&discovery_msg);
    if (!json_msg)
    {
        ESP_LOGE(Master_Tag, "Failed to create discovery JSON message. Deleting task.");
        vTaskDelete(NULL);
        return;
    }

    while (1)
    {
        if (slave_count == 0)
        {
            ESP_ERROR_CHECK(esp_wifi_set_channel(ESP_NOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
            ESP_LOGI(Master_Tag, "Sending discovery broadcast on channel %d", ESP_NOW_WIFI_CHANNEL);
            espnow_api_send_to(BROADCAST_MAC, (const uint8_t *)json_msg, strlen(json_msg));
        }
        vTaskDelay(pdMS_TO_TICKS(DISCOVERY_PERIOD_MS));
    }
    free(json_msg);
}

/**
 * @brief task receive espnow message
 * @details This task continuously listens for incoming ESP-NOW messages. Upon receiving a message, it decodes the JSON content and processes it based on the message type.
 * @param pvParameters
 */
static void espnow_receive_task(void *pvParameters)
{
    ESP_LOGI(Master_Tag, "espnow_receive_task started");
    espnow_msg_t msg;
    while (1)
    {
        if (espnow_api_recv(&msg, portMAX_DELAY) == ESP_OK)
        {
            size_t safe_len = msg.len;
            if (safe_len >= sizeof(msg.data))
            {
                safe_len = sizeof(msg.data) - 1;
            }
            msg.data[safe_len] = '\0'; // Đảm bảo chuỗi kết thúc null
            ESP_LOGI(Master_Tag, "Received from %02X:%02X:%02X:%02X:%02X:%02X: %s",
                     msg.src_mac[0], msg.src_mac[1], msg.src_mac[2], msg.src_mac[3], msg.src_mac[4], msg.src_mac[5],
                     (char *)msg.data);

            json_msg_type_t msg_type = json_decode_msg_type((const char *)msg.data);

            switch (msg_type)
            {
            case JSON_MSG_TYPE_DISCOVERY_RESPONSE:
            {
                json_discovery_response_t *resp = json_decode_discovery_response((const char *)msg.data);
                if (resp)
                {
                    ESP_LOGI(Master_Tag, "Discovered slave '%s' with MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                             resp->name,
                             msg.src_mac[0], msg.src_mac[1], msg.src_mac[2],
                             msg.src_mac[3], msg.src_mac[4], msg.src_mac[5]);
                    add_new_slave(msg.src_mac, resp->name);
                    free(resp);
                }
                break;
            }
            case JSON_MSG_TYPE_RESPONSE_DATA:
            {
                if (uart_json_queue)
                {
                    uart_json_msg_t out = {0};
                    out.evt = UART_BRIDGE_EVT_JSON;
                    out.len = (uint16_t)strnlen((const char *)msg.data, sizeof(out.json) - 1);
                    memcpy(out.json, (const char *)msg.data, out.len);
                    out.json[out.len] = '\0';

                    if (xQueueSend(uart_json_queue, &out, 0) != pdTRUE)
                    {
                        ESP_LOGW(Master_Tag, "uart_json_queue full, dropping JSON");
                    }
                }
                break;
            }
            default:
                ESP_LOGW(Master_Tag, "Received unknown or malformed JSON message type.");
                break;
            }
        }
    }
}

/**
 * @brief task request data from slaves
 * @details This task periodically sends data request messages to all discovered slaves every 1000  milliseconds.
 * @param pvParameters
 */
static void data_request_task(void *pvParameters)
{
    ESP_LOGI(Master_Tag, "data_request_task started (period=%dms)", ASK_DATA_PERIOD_MS);
    char master_mac_str[MAC_ADDR_STR_LEN];
    mac_to_string(MASTER_MAC, master_mac_str);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(ASK_DATA_PERIOD_MS)); // Gửi yêu cầu mỗi 1 giây
        if (slave_count > 0)
        {
            json_master_msg_t ask_msg = {.type = JSON_MSG_TYPE_ASK_DATA};
            strcpy(ask_msg.id, master_mac_str);

            char *json_ask_common = json_encode_master_msg(&ask_msg);
            if (!json_ask_common)
            {
                ESP_LOGE(Master_Tag, "Failed to create ASK_DATA JSON");
                continue;
            }

            // Signal UART bridge: start a new collection cycle BEFORE sending requests
            // (prevents responses arriving before the cycle marker and being dropped)
            if (uart_json_queue)
            {
                uart_json_msg_t cycle = {0};
                cycle.evt = UART_BRIDGE_EVT_CYCLE;
                (void)xQueueSend(uart_json_queue, &cycle, 0);
            }

            for (int i = 0; i < slave_count; i++)
            {
                char slave_mac_str[MAC_ADDR_STR_LEN];
                mac_to_string(discovered_slaves[i].mac, slave_mac_str);
                ESP_LOGI(Master_Tag, "ASK_DATA -> %s (%s)", discovered_slaves[i].name, slave_mac_str);
                espnow_api_send_to(discovered_slaves[i].mac, (const uint8_t *)json_ask_common, strlen(json_ask_common));
            }

            free(json_ask_common);
        }
    }
}


void app_main(void)
{
    uart_init_with_fsm(UART_BAUD_RATE, UART_TX_PIN, UART_RX_PIN);

    // Queue/task to forward ESP-NOW response_data JSON -> UART frames
    uart_json_queue = xQueueCreate(10, sizeof(uart_json_msg_t));
    if (!uart_json_queue)
    {
        ESP_LOGE(Master_Tag, "Failed to create uart_json_queue");
    }
    else
    {
        if (xTaskCreate(
                uart_bridge_task,
                "uart_bridge_task",
                4096,
                NULL,
                3,
                NULL) != pdPASS)
        {
            ESP_LOGE(Master_Tag, "Failed to create uart_bridge_task");
        }
    }

    /* ================== Wi-Fi (ESP-NOW ONLY) ================== */
    esp_err_t err = wifi_init_for_esp_now();
    if (err != ESP_OK)
    {
        ESP_LOGE(Master_Tag, "wifi_init_for_esp_now failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(Master_Tag, "Wi-Fi started (STA mode, ESP-NOW)");

    /* ================== Channel ================== */
    ESP_ERROR_CHECK(
        esp_wifi_set_channel(ESP_NOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    /* ================== MAC ================== */
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, MASTER_MAC));

    char mac_str[18];
    mac_to_string(MASTER_MAC, mac_str);
    ESP_LOGI(Master_Tag, "Master MAC: %s", mac_str);

    /* ================== ESP-NOW ================== */
    espnow_api_init();
    ESP_LOGI(Master_Tag, "ESP-NOW initialized");

    espnow_api_register_send_cb(master_send_cb);

    /* ================== Broadcast peer ================== */
    esp_err_t add_ret = espnow_api_add_peer(
        BROADCAST_MAC,
        ESP_NOW_WIFI_CHANNEL,
        false);

    if (add_ret != ESP_OK && add_ret != ESP_ERR_ESPNOW_EXIST)
    {
        ESP_LOGE(Master_Tag, "Add broadcast peer failed: %s",
                 esp_err_to_name(add_ret));
    }
    else
    {
        ESP_LOGI(Master_Tag, "Broadcast peer added");
    }

    /* ================== Tasks ================== */
    if (xTaskCreate(
            espnow_receive_task,
            "espnow_receive_task",
            4096,
            NULL,
            6,
            NULL) != pdPASS)
    {
        ESP_LOGE(Master_Tag, "Failed to create espnow_receive_task");
    }

    if (xTaskCreate(
            master_discovery_task,
            "master_discovery_task",
            4096,
            NULL,
            4,
            NULL) != pdPASS)
    {
        ESP_LOGE(Master_Tag, "Failed to create master_discovery_task");
    }

    if (xTaskCreate(
            data_request_task,
            "data_request_task",
            4096,
            NULL,
            5,
            NULL) != pdPASS)
    {
        ESP_LOGE(Master_Tag, "Failed to create data_request_task");
    }

    ESP_LOGI(Master_Tag, "=== Master ESP-NOW READY ===");
}
