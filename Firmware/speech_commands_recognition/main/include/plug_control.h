/**
 * @file plug_control.h
 * @brief Plug control module - Manage plug states and MQTT messaging
 */

#ifndef PLUG_CONTROL_H
#define PLUG_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/* ================== PLUG ENUMS ================== */
typedef enum
{
    PLUG_1 = 0,
    PLUG_2 = 1,
    PLUG_3 = 2,
} plug_id_t;

typedef enum
{
    STATE_OFF = 0,
    STATE_ON = 1,
} plug_state_t;

/* ================== PLUG STATE STRUCT ================== */
typedef struct
{
    int flag_plug_1;
    int flag_plug_2;
    int flag_plug_3;
} plug_flags_t;

/* ================== MQTT MESSAGE STRUCT ================== */
typedef struct
{
    plug_id_t plug;
    plug_state_t state;
} mqtt_msg_t;

/* ================== GLOBAL VARIABLES ================== */
extern plug_flags_t g_plug_flags;
extern QueueHandle_t g_mqtt_queue;

/* ================== FUNCTION PROTOTYPES ================== */

/**
 * @brief Initialize plug control module
 * @return ESP_OK on success
 */
esp_err_t plug_control_init(void);

/**
 * @brief Send plug control command to MQTT queue
 * @param plug Plug ID (PLUG_1, PLUG_2, PLUG_3)
 * @param state Plug state (STATE_ON, STATE_OFF)
 */
void plug_send_command(plug_id_t plug, plug_state_t state);

/**
 * @brief Publish plug control via MQTT
 * @param plug Plug ID
 * @param state Plug state
 */
void plug_publish_mqtt(plug_id_t plug, plug_state_t state);

/**
 * @brief Set plug flag state
 * @param plug Plug ID
 * @param state State to set
 */
void plug_set_flag(plug_id_t plug, plug_state_t state);

/**
 * @brief Set all plug flags
 * @param state State to set for all plugs
 */
void plug_set_all_flags(plug_state_t state);

/**
 * @brief MQTT task function
 * @param arg Task argument (unused)
 */
void mqtt_task(void *arg);

#endif /* PLUG_CONTROL_H */
