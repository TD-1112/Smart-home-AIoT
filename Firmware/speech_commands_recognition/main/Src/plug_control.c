/**
 * @file plug_control.c
 * @brief Plug control module implementation
 */

#include "plug_control.h"
#include "app_config.h"
#include "my_mqtt.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "[PLUG]";

/* ================== GLOBAL VARIABLES ================== */
plug_flags_t g_plug_flags = {
    .flag_plug_1 = 0,
    .flag_plug_2 = 0,
    .flag_plug_3 = 0};

QueueHandle_t g_mqtt_queue = NULL;

/* ================== MQTT CONFIG ================== */
static my_mqtt_init_t mqtt_cfg = {
    .server = MQTT_BROKER,
    .topic_pub = MQTT_TOPIC_PUB,
    .topic_sub = MQTT_TOPIC_SUB};

/* ================== HELPER FUNCTIONS ================== */
static const char *get_plug_name(plug_id_t plug)
{
    switch (plug)
    {
    case PLUG_1:
        return "plug_1";
    case PLUG_2:
        return "plug_2";
    case PLUG_3:
        return "plug_3";
    default:
        return NULL;
    }
}

/* ================== PUBLIC FUNCTIONS ================== */
esp_err_t plug_control_init(void)
{
    // Create MQTT queue
    g_mqtt_queue = xQueueCreate(MQTT_QUEUE_SIZE, sizeof(mqtt_msg_t));
    if (g_mqtt_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create MQTT queue");
        return ESP_FAIL;
    }

    // Initialize MQTT
    esp_err_t err = my_mqtt_init(&mqtt_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "MQTT init failed");
        return err;
    }

    ESP_LOGI(TAG, "Plug control initialized");
    return ESP_OK;
}

void plug_send_command(plug_id_t plug, plug_state_t state)
{
    mqtt_msg_t msg = {
        .plug = plug,
        .state = state};

    if (xQueueSend(g_mqtt_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGW(TAG, "MQTT queue full, message dropped");
    }
}

void plug_publish_mqtt(plug_id_t plug, plug_state_t state)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return;
    }

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
    {
        ESP_LOGE(TAG, "Failed to create data JSON object");
        cJSON_Delete(root);
        return;
    }

    const char *plug_name = get_plug_name(plug);
    if (plug_name == NULL)
    {
        cJSON_Delete(root);
        cJSON_Delete(data);
        ESP_LOGE(TAG, "Invalid plug");
        return;
    }

    // Build JSON: {"type":"control","data":{"plug":"plug_1","status":"on"}}
    cJSON_AddStringToObject(root, "type", "control");
    cJSON_AddStringToObject(data, "plug", plug_name);
    cJSON_AddStringToObject(data, "status", state == STATE_ON ? "on" : "off");
    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str != NULL)
    {
        ESP_LOGI(TAG, "Publishing: %s", json_str);
        my_mqtt_pub(mqtt_cfg.topic_pub, json_str);
        free(json_str);
    }

    cJSON_Delete(root);
}

void plug_set_flag(plug_id_t plug, plug_state_t state)
{
    switch (plug)
    {
    case PLUG_1:
        g_plug_flags.flag_plug_1 = state;
        break;
    case PLUG_2:
        g_plug_flags.flag_plug_2 = state;
        break;
    case PLUG_3:
        g_plug_flags.flag_plug_3 = state;
        break;
    default:
        break;
    }
}

void plug_set_all_flags(plug_state_t state)
{
    g_plug_flags.flag_plug_1 = state;
    g_plug_flags.flag_plug_2 = state;
    g_plug_flags.flag_plug_3 = state;
}

void mqtt_task(void *arg)
{
    ESP_LOGI(TAG, "MQTT task started");
    mqtt_msg_t msg;

    while (1)
    {
        if (xQueueReceive(g_mqtt_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            plug_publish_mqtt(msg.plug, msg.state);
            vTaskDelay(pdMS_TO_TICKS(50)); // Small delay between publishes
        }
    }

    vTaskDelete(NULL);
}
