/**
 * @file app_config.h
 * @brief Application configuration - Pins, WiFi, MQTT settings
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "driver/gpio.h"

/* ================== I2S PIN ================== */
#define I2S_WS GPIO_NUM_4
#define I2S_SCK GPIO_NUM_5
#define I2S_SD GPIO_NUM_6

/* ================== LED PIN ================== */
#define GPIO_LED GPIO_NUM_2

/* ================== WiFi CONFIG ================== */
#define WIFI_SSID "thang"
#define WIFI_PASSWORD "11122004"

/* ================== MQTT CONFIG ================== */
#define MQTT_BROKER "mqtt://broker.emqx.io"
#define MQTT_TOPIC_PUB "GateWays/Server"
#define MQTT_TOPIC_SUB "Server/Gateways"

/* ================== QUEUE CONFIG ================== */
#define MQTT_QUEUE_SIZE 10
#define MN_QUEUE_SIZE 5

/* ================== TASK CONFIG ================== */
#define FEED_TASK_STACK_SIZE 4096
#define DETECT_TASK_STACK_SIZE 8192
#define MQTT_TASK_STACK_SIZE 4096

#define FEED_TASK_PRIORITY 5
#define DETECT_TASK_PRIORITY 5
#define MQTT_TASK_PRIORITY 4

#define FEED_TASK_CORE 0
#define DETECT_TASK_CORE 1
#define MQTT_TASK_CORE 0

#endif /* APP_CONFIG_H */
