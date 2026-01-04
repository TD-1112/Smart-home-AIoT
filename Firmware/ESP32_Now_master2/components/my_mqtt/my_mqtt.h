#ifndef MY_MQTT_H
#define MY_MQTT_H

#include "mqtt_client.h"
#include "esp_log.h"
#include "string.h"
#include "stdbool.h"

#define TAG_MQTT "[MY_MQTT]"
#define TAG_ERROR "[ERROR_MQTT]"

typedef struct
{
    char server[128];
    char topic_pub[64];
    char topic_sub[64];
} my_mqtt_init_t;

typedef struct
{
    char topic[64];
    char payload[256];
} my_mqtt_message_t;

/**
 * @brief initialize MQTT client 
 * 
 * @param struct init cfg 
 */
void my_mqtt_init(my_mqtt_init_t *cfg);

/**
 * @brief publish message
 * 
 * @param topic 
 * @param msg 
 */
void my_mqtt_pub(const char *topic, const char *msg);

/**
 * @brief get received message (not processed in callback)
 * 
 * @param out_msg 
 * @return true 
 * @return false 
 */
bool my_mqtt_getmess(my_mqtt_message_t *out_msg);

#endif
