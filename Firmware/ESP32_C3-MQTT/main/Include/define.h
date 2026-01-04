#ifndef DEFINE_H
#define DEFINE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "my_mqtt.h"
#include "message.h"

// Pin definitions
#define UART_TX_PIN 10
#define UART_RX_PIN 9
#define LED_PIN 2

// UART config
#define UART_BAUD_RATE 115200

// Queues
extern QueueHandle_t json_queue;
extern QueueHandle_t mqtt_rx_queue;

// Global variables
extern my_mqtt_init_t mqtt_cfg;
extern Frame_Message mess;

// Task tags
extern const char *UART_TAG;
extern const char *MQTT_TAG;
extern const char *MAIN_TAG;

#endif // DEFINE_H
