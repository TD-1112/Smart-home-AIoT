#include "Json_message.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "JSON";

/**
 * @brief Get the type from string object
 * @details Converts a string representation of message type to its corresponding json_msg_type_t enum.
 * @param type_str
 * @return json_msg_type_t
 */
static json_msg_type_t get_type_from_string(const char *type_str)
{
    if (strcmp(type_str, "discovery") == 0)
        return JSON_MSG_TYPE_DISCOVERY;
    if (strcmp(type_str, "ask_data") == 0)
        return JSON_MSG_TYPE_ASK_DATA;
    if (strcmp(type_str, "discovery_response") == 0)
        return JSON_MSG_TYPE_DISCOVERY_RESPONSE;
    if (strcmp(type_str, "response_data") == 0)
        return JSON_MSG_TYPE_RESPONSE_DATA;
    return JSON_MSG_TYPE_UNKNOWN; // Add a default return
}

/**
 * @brief Get the string from type object
 * @details Converts a json_msg_type_t enum to its corresponding string representation.
 * @param type
 * @return const char*
 */
static const char *get_string_from_type(json_msg_type_t type)
{
    switch (type)
    {
    case JSON_MSG_TYPE_DISCOVERY:
        return "discovery";
    case JSON_MSG_TYPE_DISCOVERY_RESPONSE:
        return "discovery_response";
    case JSON_MSG_TYPE_ASK_DATA:
        return "ask_data";
    case JSON_MSG_TYPE_RESPONSE_DATA:
        return "response_data";
    default:
        return "unknown";
    }
}

/**
 * @brief Decode a JSON string into a master message structure.
 * @details Parses the JSON string and fills the json_master_msg_t structure.
 * @param json_str
 * @return json_master_msg_t*
 */
json_master_msg_t *json_decode_master_msg(const char *json_str)
{
    cJSON *json = cJSON_Parse(json_str);
    if (json == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON string.");
        return NULL;
    }

    cJSON *type_item = cJSON_GetObjectItem(json, "TYPE");
    cJSON *id_item = cJSON_GetObjectItem(json, "ID");

    if (!cJSON_IsString(type_item) || !cJSON_IsString(id_item))
    {
        ESP_LOGE(TAG, "JSON message missing 'TYPE' or 'ID' fields.");
        cJSON_Delete(json);
        return NULL;
    }

    json_master_msg_t *msg = (json_master_msg_t *)malloc(sizeof(json_master_msg_t));
    if (msg == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for master message.");
        cJSON_Delete(json);
        return NULL;
    }

    msg->type = get_type_from_string(type_item->valuestring);
    strncpy(msg->id, id_item->valuestring, MAC_STR_LEN - 1);
    msg->id[MAC_STR_LEN - 1] = '\0';

    cJSON_Delete(json);
    return msg;
}

/**
 * @brief Encode a slave discovery response into a JSON string.
 * @details Converts the json_slave_disc_resp_t structure into a JSON formatted string.
 * @param disc_resp
 * @return char*
 */
char *json_encode_slave_discovery_response(const json_slave_disc_resp_t *disc_resp)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to create cJSON object.");
        return NULL;
    }

    cJSON_AddStringToObject(root, "TYPE", get_string_from_type(disc_resp->type));
    cJSON_AddStringToObject(root, "ID", disc_resp->id);
    cJSON_AddStringToObject(root, "NAME", disc_resp->name);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}

char *json_encode_slave_data(const json_slave_data_t *data_resp)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to create cJSON object.");
        return NULL;
    }

    cJSON_AddStringToObject(root, "TYPE", get_string_from_type(data_resp->type));
    cJSON_AddStringToObject(root, "ID", data_resp->id);
    cJSON_AddStringToObject(root, "DST", data_resp->dst);

    cJSON *data_obj = cJSON_CreateObject();
    if (data_obj == NULL)
    {
        ESP_LOGE(TAG, "Failed to create data sub-object.");
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "data", data_obj);
    cJSON_AddNumberToObject(data_obj, "temp", data_resp->data.temp);
    cJSON_AddNumberToObject(data_obj, "humi", data_resp->data.humi);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str;
}