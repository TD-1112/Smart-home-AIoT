#include "lib_uart.h"
#include "fsm.h"
#include "lib_math.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UART_LIB";

static QueueHandle_t uart_queue = NULL;
static TaskHandle_t uart_rx_task_handle = NULL;
static void (*uart_rx_callback)(uint8_t data) = NULL; // callback giống ngắt UART

// ======================= Internal Task =======================
static void uart_rx_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t data;

    while (1)
    {
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY))
        {
            if (event.type == UART_DATA)
            {
                while (uart_read_bytes(UART_PORT_NUM, &data, 1, 10 / portTICK_PERIOD_MS))
                {
                    if (uart_rx_callback)
                        uart_rx_callback(data);
                }
            }
        }
    }
}

// ======================= Basic operations =======================
void uart_basic_init(uint32_t baud_rate, gpio_num_t tx_pin, gpio_num_t rx_pin)
{
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT};

    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT_NUM, BUFFER_SIZE * 2, BUFFER_SIZE * 2, 10, &uart_queue, 0);

    xTaskCreate(uart_rx_task, "uart_rx_task", 2048, NULL, 12, &uart_rx_task_handle);
    ESP_LOGI(TAG, "UART initialized (TX=%d, RX=%d, baud=%lu)", tx_pin, rx_pin, baud_rate);
}

void uart_basic_deinit(void)
{
    if (uart_rx_task_handle)
    {
        vTaskDelete(uart_rx_task_handle);
        uart_rx_task_handle = NULL;
    }
    uart_driver_delete(UART_PORT_NUM);
    ESP_LOGI(TAG, "UART deinitialized");
}

void uart_basic_flush(void)
{
    uart_flush(UART_PORT_NUM);
}

// ======================= Send operations =======================
void uart_send_byte(uint8_t data)
{
    uart_write_bytes(UART_PORT_NUM, (const char *)&data, 1);
}

void uart_send_bytes(const uint8_t *data, size_t length)
{
    uart_write_bytes(UART_PORT_NUM, (const char *)data, length);
}

// ======================= Receive operations =======================
int uart_receive_available(void)
{
    size_t available;
    uart_get_buffered_data_len(UART_PORT_NUM, &available);
    return (int)available;
}

int uart_receive_bytes(uint8_t *buffer, size_t length, uint32_t timeout_ms)
{
    return uart_read_bytes(UART_PORT_NUM, buffer, length, timeout_ms / portTICK_PERIOD_MS);
}

uint8_t uart_receive_byte(uint32_t timeout_ms)
{
    uint8_t byte = 0;
    uart_read_bytes(UART_PORT_NUM, &byte, 1, timeout_ms / portTICK_PERIOD_MS);
    return byte;
}

// ======================= Callback registration =======================
void uart_set_rx_callback(void (*callback)(uint8_t data))
{
    uart_rx_callback = callback;
}

// ======================= FSM integration =======================
static void uart_fsm_callback(uint8_t data)
{
    // Debug: In ra mỗi byte nhận được
    // ESP_LOGI(TAG, "RX Byte: 0x%02X (%c)", data, (data >= 32 && data <= 126) ? data : '.');
    fsm_get_message(data, fsm_message_buffer);
}

void uart_init_with_fsm(uint32_t baud_rate, gpio_num_t tx_pin, gpio_num_t rx_pin)
{
    uart_basic_init(baud_rate, tx_pin, rx_pin);
    uart_set_rx_callback(uart_fsm_callback);
    ESP_LOGI(TAG, "UART + FSM initialized");
}

// ======================= Check message =======================
uint8_t is_message(void)
{
    uint16_t dummy_length = 0;
    uint8_t result = (uint8_t)Is_Message(&dummy_length);
    if (result)
    {
        ESP_LOGI(TAG, "FSM: Frame complete! Length=%d", dummy_length);
    }
    return result;
}

// ======================= Decode message =======================
void decode_message(Frame_Message *message)
{
    // Decode start_message (0xAA55) từ byte 0-1
    message->start_message = math.convert.bytes_to_uint16(fsm_message_buffer[0], fsm_message_buffer[1]);

    // Decode type_message từ byte 2
    message->type_message = (Type_Message)fsm_message_buffer[2];

    // Decode length_message từ byte 3-4
    message->length_message = math.convert.bytes_to_uint16(fsm_message_buffer[3], fsm_message_buffer[4]);

    // Copy data payload (bắt đầu từ byte 5)
    for (uint8_t i = 0; i < message->length_message - FRAME_HEADER_SIZE; i++)
    {
        message->data[i] = fsm_message_buffer[i + FRAME_HEADER_SIZE];
    }

    // Decode checksum (2 byte cuối cùng)
    uint16_t checksum_index = FRAME_HEADER_SIZE + (message->length_message - FRAME_HEADER_SIZE);
    message->check_sum = math.convert.bytes_to_uint16(fsm_message_buffer[checksum_index], fsm_message_buffer[checksum_index + 1]);
}

// ======================= Library interface table =======================
const uart_lib_t uart = {
    .basic = {
        .init = uart_basic_init,
        .deinit = uart_basic_deinit,
        .flush = uart_basic_flush,
    },
    .send = {
        .byte = uart_send_byte,
        .bytes = uart_send_bytes,
    },
    .receive = {
        .available = uart_receive_available,
        .bytes = uart_receive_bytes,
        .byte = uart_receive_byte,
    },
    .check = {
        .is_message = is_message,
    }};
