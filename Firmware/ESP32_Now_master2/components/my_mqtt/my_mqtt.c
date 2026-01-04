#include "my_mqtt.h"

static esp_mqtt_client_handle_t client = NULL;
static my_mqtt_message_t last_msg;
static bool msg_received = false;
static my_mqtt_init_t mqtt_cfg_local;


/**
 * @brief callback MQTT event handler
 * 
 * @param handler_args 
 * @param base 
 * @param event_id 
 * @param event_data 
 */
static void my_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG_MQTT, "Connected to broker: %s", mqtt_cfg_local.server);
        esp_mqtt_client_subscribe(client, mqtt_cfg_local.topic_sub, 0);
        ESP_LOGI(TAG_MQTT, "Subscribed to topic: %s", mqtt_cfg_local.topic_sub);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG_ERROR, "MQTT disconnected! Reconnecting...");
        esp_mqtt_client_reconnect(client);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG_MQTT, "Message received:");
        ESP_LOGI(TAG_MQTT, "  Topic: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG_MQTT, "  Data : %.*s", event->data_len, event->data);

        memset(&last_msg, 0, sizeof(last_msg));
        snprintf(last_msg.topic, sizeof(last_msg.topic), "%.*s", event->topic_len, event->topic);
        snprintf(last_msg.payload, sizeof(last_msg.payload), "%.*s", event->data_len, event->data);
        msg_received = true;
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG_ERROR, "MQTT event error!");
        break;

    default:
        break;
    }
}


/**
 * @brief initialize MQTT client
 * 
 * @param cfg 
 */
void my_mqtt_init(my_mqtt_init_t *cfg)
{
    mqtt_cfg_local = *cfg;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = cfg->server,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, my_mqtt_event_handler, NULL);

    esp_err_t ret = esp_mqtt_client_start(client);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_ERROR, "Failed to start MQTT client! Error code: %d", ret);
    }
}


/**
 * @brief publish message
 * 
 * @param topic 
 * @param msg 
 */
void my_mqtt_pub(const char *topic, const char *msg)
{
    if (client)
    {
        int msg_id = esp_mqtt_client_publish(client, topic, msg, 0, 1, 0);
        if (msg_id >= 0)
            ESP_LOGI(TAG_MQTT, "Published -> topic: %s | msg: %s", topic, msg);
        else
            ESP_LOGE(TAG_ERROR, "Failed to publish message!");
    }
    else
    {
        ESP_LOGE(TAG_ERROR, "MQTT client not initialized!");
    }
}


/**
 * @brief get received message (not processed in callback)
 * 
 * @param out_msg 
 * @return true 
 * @return false 
 */
bool my_mqtt_getmess(my_mqtt_message_t *out_msg)
{
    if (msg_received)
    {
        *out_msg = last_msg;
        msg_received = false;
        return true;
    }
    return false;
}
