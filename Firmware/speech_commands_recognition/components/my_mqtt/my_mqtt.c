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

        // Call the callback from the config struct if it's provided
        if (mqtt_cfg_local.on_connected_cb != NULL)
        {
            mqtt_cfg_local.on_connected_cb();
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG_ERROR, "MQTT disconnected! Reconnecting...");
        // The client will try to reconnect automatically. 
        // We can also force it here if needed.
        // esp_mqtt_client_reconnect(client);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG_MQTT, "Message received:");
        ESP_LOGI(TAG_MQTT, "  Topic: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG_MQTT, "  Data : %.*s", event->data_len, event->data);

        // Safely copy data to the message buffer
        memset(&last_msg, 0, sizeof(last_msg));
        last_msg.topic_len = event->topic_len < sizeof(last_msg.topic) ? event->topic_len : sizeof(last_msg.topic) - 1;
        strncpy(last_msg.topic, event->topic, last_msg.topic_len);
        
        last_msg.payload_len = event->data_len < sizeof(last_msg.payload) ? event->data_len : sizeof(last_msg.payload) - 1;
        strncpy(last_msg.payload, event->data, last_msg.payload_len);
        
        msg_received = true;
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG_ERROR, "MQTT event error!");
        if (event->error_handle) {
            ESP_LOGE(TAG_ERROR, "Last error code: 0x%x", event->error_handle->error_type);
            ESP_LOGE(TAG_ERROR, "Last ESP-IDF error code: 0x%x", event->error_handle->esp_tls_last_esp_err);
        }
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
esp_err_t my_mqtt_init(my_mqtt_init_t *cfg)
{
    mqtt_cfg_local = *cfg;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = cfg->server,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, my_mqtt_event_handler, NULL);

    esp_err_t ret = esp_mqtt_client_start(client);

    return ret;
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
