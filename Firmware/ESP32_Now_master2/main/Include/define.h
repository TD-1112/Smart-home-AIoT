#ifndef DEFINE_H
#define DEFINE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "Json_message.h"

// ----- define -----
/*pin uart2 on esp32-wrom*/
#define UART_TX_PIN 17
#define UART_RX_PIN 16
#define UART_BAUD_RATE 115200

#define DISCOVERY_PERIOD_MS 5000     // send discovery every 5 seconds
#define MQTT_PUBLISH_PERIOD_MS 10000 // send data to MQTT every 10 seconds
#define ASK_DATA_PERIOD_MS 1000      // send request data every 1 second
#define ESP_NOW_WIFI_CHANNEL 1       // Fixed channel for ESP-NOW (must match Slave)
#define MAX_SLAVES 10

// Struct used to store discovered slave information
typedef struct
{
    uint8_t mac[6];
    char name[SLAVE_NAME_LEN];
} discovered_slave_t;

typedef enum
{
    UART_BRIDGE_EVT_JSON = 0,
    UART_BRIDGE_EVT_CYCLE = 1,
} uart_bridge_evt_t;

typedef struct
{
    uart_bridge_evt_t evt;
    uint16_t len;
    char json[251]; // espnow_msg_t.data max 250 + null terminator
} uart_json_msg_t;


// ----- global variables -----
extern const char *Master_Tag;
extern const uint8_t BROADCAST_MAC[6];
extern uint8_t MASTER_MAC[6];
extern discovered_slave_t discovered_slaves[MAX_SLAVES];
extern volatile int slave_count;
extern QueueHandle_t uart_json_queue;


#endif // DEFINE_H
