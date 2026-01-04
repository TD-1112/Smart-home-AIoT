#include "esp_log.h"
#include "api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include "lib_uart.h"
#include "lib_math.h"
#include "fsm.h"
#include "message.h"

#define UART_TX_PIN 10
#define UART_RX_PIN 9
#define UART_BAUD_RATE 115200

// Task priorities
#define TASK_SEND_PRIORITY 2
#define TASK_RECV_PRIORITY 3
#define TASK_ESPNOW_SEND_PRIORITY 2

#define TASK_SEND_STACK_SIZE 2048
#define TASK_RECV_STACK_SIZE 2048
#define TASK_ESPNOW_SEND_STACK_SIZE 2048

#define SEND_TASK_DELAY_MS 10
#define ESPNOW_DATA_QUEUE_LEN 10

static const char *TAG = "MASTER";
static const char *TAG_SEND = "SEND_TASK";
static const char *TAG_RECV = "RECV_TASK";
static const char *TAG_ESPNOW = "ESPNOW_SEND";

static const uint8_t MASTER_MAC[6] = {0x30, 0xae, 0xa4, 0x98, 0x9f, 0x84};

Frame_Message mess;

QueueHandle_t espnow_data_queue = NULL;

typedef struct
{
    uint16_t value;
    uint8_t type_message;
} espnow_data_t;

/**
 * @brief Task to send ASK messages periodically
 * @details This task constructs and sends ASK messages via UART at regular intervals.
 * @param pvParameters
 */
void task_send_ask(void *pvParameters)
{
    ESP_LOGI(TAG_SEND, "Task Send ASK started");

    uint8_t ask_message[10] = {0};
    uint16_t ask_length = create_message(ASK_MESSAGE, 0, ask_message);

    while (true)
    {
        uart.send.bytes(ask_message, ask_length);
        vTaskDelay(SEND_TASK_DELAY_MS / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Task to receive and process RESPONSE messages
 * @details This task listens for incoming messages via UART, decodes them,
 *          and sends the relevant data to the ESP-NOW queue.
 * @param pvParameters
 */
void task_receive_response(void *pvParameters)
{
    ESP_LOGI(TAG_RECV, "Task Receive Response started");

    while (true)
    {
        if (uart.check.is_message())
        {
            decode_message(&mess);
            if (mess.type_message == RESPONSE_MESSAGE)
            {
                uint16_t value = math.convert.bytes_to_uint16(mess.data[1], mess.data[0]);
                ESP_LOGI(TAG_RECV, "Decoded value: %d (0x%04X)", value, value);

                espnow_data_t data_to_send = {
                    .value = value,
                    .type_message = mess.type_message};

                if (xQueueSend(espnow_data_queue, &data_to_send, pdMS_TO_TICKS(100)) == pdTRUE)
                {
                    ESP_LOGI(TAG_RECV, "Data sent to ESP-NOW queue");
                }
                else
                {
                    ESP_LOGW(TAG_RECV, "Failed to send data to ESP-NOW queue (queue full)");
                }
            }
            else
            {
                ESP_LOGW(TAG_RECV, "Received unknown message type: 0x%02X", mess.type_message);
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Task to send data via ESP-NOW
 * @details This task retrieves data from a queue and sends it using the ESP-NOW protocol.
 * @param pvParameters
 */
void task_espnow_send(void *pvParameters)
{
    ESP_LOGI(TAG_ESPNOW, "Task ESP-NOW Send started");

    espnow_data_t received_data;

    while (true)
    {
        if (xQueueReceive(espnow_data_queue, &received_data, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            uint8_t esp_now_payload[10] = {0};
            uint16_t payload_len = 0;

            // Format: [Type(1 byte)] [Value_High(1 byte)] [Value_Low(1 byte)]
            esp_now_payload[payload_len++] = received_data.type_message;

            // Convert giá trị thành 2 bytes (Big-endian hoặc Little-endian tùy yêu cầu)
            esp_now_payload[payload_len++] = (uint8_t)((received_data.value >> 8) & 0xFF); // High byte
            esp_now_payload[payload_len++] = (uint8_t)(received_data.value & 0xFF);        // Low byte

            // Gửi dữ liệu qua ESP-NOW
            esp_err_t ret = espnow_api_send(esp_now_payload, payload_len);

            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG_ESPNOW, "ESP-NOW send successful");
            }
            else
            {
                ESP_LOGE(TAG_ESPNOW, "ESP-NOW send failed: %s", esp_err_to_name(ret));
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// ==================== Main Application ====================
void app_main(void)
{
    // Khởi tạo UART với FSM
    uart_init_with_fsm(UART_BAUD_RATE, UART_TX_PIN, UART_RX_PIN);
    ESP_LOGI(TAG, "UART initialized");

    espnow_data_queue = xQueueCreate(ESPNOW_DATA_QUEUE_LEN, sizeof(espnow_data_t));
    if (espnow_data_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create ESP-NOW data queue");
        return;
    }
    ESP_LOGI(TAG, "ESP-NOW data queue created");

    espnow_api_init(MASTER_MAC);
    ESP_LOGI(TAG, "ESP-NOW API initialized");

    xTaskCreate(
        task_send_ask,
        "send_ask_task",
        TASK_SEND_STACK_SIZE,
        NULL,
        TASK_SEND_PRIORITY,
        NULL);
    ESP_LOGI(TAG, "Send ASK task created");

    xTaskCreate(
        task_receive_response,
        "recv_resp_task",
        TASK_RECV_STACK_SIZE,
        NULL,
        TASK_RECV_PRIORITY,
        NULL);
    ESP_LOGI(TAG, "Receive Response task created");

    xTaskCreate(
        task_espnow_send,
        "espnow_send_task",
        TASK_ESPNOW_SEND_STACK_SIZE,
        NULL,
        TASK_ESPNOW_SEND_PRIORITY,
        NULL);
    ESP_LOGI(TAG, "ESP-NOW Send task created");
    while (true)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
