#ifndef __LIB_UART__
#define __LIB_UART__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "message.h"

// ================= Configuration =================
#define UART_PORT_NUM UART_NUM_1
#define BUFFER_SIZE 256

// =================================================

typedef struct
{
    // Basic UART operations
    struct
    {
        void (*init)(uint32_t baud_rate, gpio_num_t tx_pin, gpio_num_t rx_pin);
        void (*deinit)(void);
        void (*flush)(void);
    } basic;

    // Send operations
    struct
    {
        void (*byte)(uint8_t data);
        void (*bytes)(const uint8_t *data, size_t length);
    } send;

    // Receive operations
    struct
    {
        int (*available)(void);
        int (*bytes)(uint8_t *buffer, size_t length, uint32_t timeout_ms);
        uint8_t (*byte)(uint32_t timeout_ms);
    } receive;

    // Check functions (message-based)
    struct
    {
        uint8_t (*is_message)(void);
    } check;

} uart_lib_t;

extern const uart_lib_t uart;

// ================= Optional FSM interface =================
// Callback nhận từng byte (giống ISR trong STM8)
void uart_set_rx_callback(void (*callback)(uint8_t data));

// Hàm khởi tạo UART có gắn FSM
void uart_init_with_fsm(uint32_t baud_rate, gpio_num_t tx_pin, gpio_num_t rx_pin);

// Hàm decode message từ buffer FSM
void decode_message(Frame_Message *message);

#endif // __LIB_UART__
