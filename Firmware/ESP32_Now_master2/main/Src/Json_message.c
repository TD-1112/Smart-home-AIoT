#include "Json_message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Helper Macros ---
#define JSON_SAFE_FREE(p) \
    do                    \
    {                     \
        if (p)            \
        {                 \
            free(p);      \
            p = NULL;     \
        }                 \
    } while (0)

// --- Get Message Type ---
json_msg_type_t json_decode_msg_type(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root)
        return JSON_MSG_TYPE_UNKNOWN;

    cJSON *type_item = cJSON_GetObjectItem(root, "TYPE");
    if (!cJSON_IsString(type_item))
    {
        cJSON_Delete(root);
        return JSON_MSG_TYPE_UNKNOWN;
    }

    char *type_str = type_item->valuestring;
    json_msg_type_t type = JSON_MSG_TYPE_UNKNOWN;

    if (strcmp(type_str, "discovery") == 0)
    {
        type = JSON_MSG_TYPE_DISCOVERY;
    }
    else if (strcmp(type_str, "discovery_response") == 0)
    {
        type = JSON_MSG_TYPE_DISCOVERY_RESPONSE;
    }
    else if (strcmp(type_str, "ask_data") == 0)
    {
        type = JSON_MSG_TYPE_ASK_DATA;
    }
    else if (strcmp(type_str, "control") == 0)
    {
        type = JSON_MSG_TYPE_CONTROL;
    }
    else if (strcmp(type_str, "response_data") == 0)
    {
        type = JSON_MSG_TYPE_RESPONSE_DATA;
    }

    cJSON_Delete(root);
    return type;
}

// --- Encode Master Message ---
char *json_encode_master_msg(const json_master_msg_t *msg)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    const char *type_str = "";
    switch (msg->type)
    {
    case JSON_MSG_TYPE_DISCOVERY:
        type_str = "discovery";
        // For discovery, ID and DST might not be needed, or ID is master's and DST is broadcast
        break;
    case JSON_MSG_TYPE_ASK_DATA:
        type_str = "ask_data";
        break;
    case JSON_MSG_TYPE_CONTROL:
        type_str = "control";
        break;
    default:
        cJSON_Delete(root);
        return NULL; // Or handle error appropriately
    }

    cJSON_AddStringToObject(root, "TYPE", type_str);

    // ID and DST are optional for discovery broadcast, but good practice to include
    if (msg->id[0] != '\0')
    {
        cJSON_AddStringToObject(root, "ID", msg->id);
    }
    if (msg->dst[0] != '\0')
    {
        cJSON_AddStringToObject(root, "DST", msg->dst);
    }

    if (msg->type == JSON_MSG_TYPE_CONTROL && msg->has_cmd)
    {
        cJSON *cmd_obj = cJSON_CreateObject();
        if (msg->cmd == JSON_CMD_TURN_ON_LED)
        {
            cJSON_AddStringToObject(cmd_obj, "CMD", "turn on led");
        }
        else if (msg->cmd == JSON_CMD_TURN_OFF_LED)
        {
            cJSON_AddStringToObject(cmd_obj, "CMD", "turn off led");
        }
        else if (msg->cmd == JSON_CMD_REGISTER_SUCCESS)
        {
            cJSON_AddStringToObject(cmd_obj, "CMD", "register_success");
        }
        cJSON_AddItemToObject(root, "CMD", cmd_obj);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

// --- Decode Discovery Response ---
json_discovery_response_t *json_decode_discovery_response(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root)
        return NULL;

    cJSON *type = cJSON_GetObjectItem(root, "TYPE");
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "discovery_response") != 0)
    {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *id = cJSON_GetObjectItem(root, "ID");
    cJSON *name = cJSON_GetObjectItem(root, "NAME");

    if (!cJSON_IsString(id) || !id->valuestring ||
        !cJSON_IsString(name) || !name->valuestring)
    {
        cJSON_Delete(root);
        return NULL;
    }

    json_discovery_response_t *resp = calloc(1, sizeof(json_discovery_response_t));
    if (!resp)
    {
        cJSON_Delete(root);
        return NULL;
    }

    snprintf(resp->id, MAC_ADDR_STR_LEN, "%s", id->valuestring);
    snprintf(resp->name, SLAVE_NAME_LEN, "%s", name->valuestring);

    cJSON_Delete(root);
    return resp;
}

// --- Decode Master Message ---
json_master_msg_t *json_decode_master_msg(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root)
        return NULL;
    cJSON *id = cJSON_GetObjectItem(root, "ID");
    cJSON *dst = cJSON_GetObjectItem(root, "DST");
    cJSON *type_item = cJSON_GetObjectItem(root, "TYPE");
    if (!cJSON_IsString(id) || !cJSON_IsString(dst) || !cJSON_IsString(type_item))
    {
        cJSON_Delete(root);
        return NULL;
    }
    json_master_msg_t *msg = (json_master_msg_t *)calloc(1, sizeof(json_master_msg_t));
    if (!msg)
    {
        cJSON_Delete(root);
        return NULL;
    }
    strncpy(msg->id, id->valuestring, MAC_ADDR_STR_LEN - 1);
    strncpy(msg->dst, dst->valuestring, MAC_ADDR_STR_LEN - 1);
    msg->id[MAC_ADDR_STR_LEN - 1] = '\0';
    msg->dst[MAC_ADDR_STR_LEN - 1] = '\0';

    msg->type = json_decode_msg_type(json_str);

    if (msg->type == JSON_MSG_TYPE_CONTROL)
    {
        cJSON *cmd_obj = cJSON_GetObjectItem(root, "CMD");
        if (cmd_obj && cJSON_IsObject(cmd_obj))
        {
            cJSON *cmd = cJSON_GetObjectItem(cmd_obj, "CMD");
            if (cJSON_IsString(cmd))
            {
                if (strcmp(cmd->valuestring, "turn on led") == 0)
                {
                    msg->cmd = JSON_CMD_TURN_ON_LED;
                    msg->has_cmd = true;
                }
                else if (strcmp(cmd->valuestring, "turn off led") == 0)
                {
                    msg->cmd = JSON_CMD_TURN_OFF_LED;
                    msg->has_cmd = true;
                }
                else if (strcmp(cmd->valuestring, "register_success") == 0)
                {
                    msg->cmd = JSON_CMD_REGISTER_SUCCESS;
                    msg->has_cmd = true;
                }
            }
        }
    }

    cJSON_Delete(root);
    return msg;
}

// --- Encode Slave Message ---
char *json_encode_slave_msg(const json_slave_msg_t *msg)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;
    cJSON_AddStringToObject(root, "ID", msg->id);
    cJSON_AddStringToObject(root, "DST", msg->dst);
    cJSON_AddStringToObject(root, "TYPE", "response_data");
    cJSON *data_obj = cJSON_CreateObject();
    if (msg->is_dht11)
    {
        cJSON_AddNumberToObject(data_obj, "temp", msg->data.dht11.temp);
        cJSON_AddNumberToObject(data_obj, "humi", msg->data.dht11.humi);
    }
    else
    {
        cJSON_AddNumberToObject(data_obj, "lux", msg->data.lux_sensor.lux);
    }
    cJSON_AddItemToObject(root, "data", data_obj);
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

json_slave_msg_t *json_decode_slave_msg(const char *json_str)
{
    if (!json_str)
        return NULL;

    cJSON *root = cJSON_Parse(json_str);
    if (!root)
        return NULL;

    json_slave_msg_t *msg = NULL;

    /* ===== BASIC FIELDS ===== */
    cJSON *id = cJSON_GetObjectItem(root, "ID");
    cJSON *dst = cJSON_GetObjectItem(root, "DST");
    cJSON *type = cJSON_GetObjectItem(root, "TYPE");
    cJSON *data = cJSON_GetObjectItem(root, "data");

    if (!cJSON_IsString(id) || !id->valuestring ||
        !cJSON_IsString(dst) || !dst->valuestring ||
        !cJSON_IsString(type) || !type->valuestring ||
        !cJSON_IsObject(data))
    {
        goto exit;
    }

    msg = calloc(1, sizeof(json_slave_msg_t));
    if (!msg)
        goto exit;

    /* ===== COPY STRING SAFE ===== */
    snprintf(msg->id, MAC_ADDR_STR_LEN, "%s", id->valuestring);
    snprintf(msg->dst, MAC_ADDR_STR_LEN, "%s", dst->valuestring);

    /* ===== TYPE ===== */
    if (strcmp(type->valuestring, "response_data") == 0)
    {
        msg->type = JSON_MSG_TYPE_RESPONSE_DATA;
    }
    else
    {
        goto error;
    }

    /* ===== DATA ===== */
    cJSON *temp = cJSON_GetObjectItem(data, "temp");
    cJSON *humi = cJSON_GetObjectItem(data, "humi");
    cJSON *lux = cJSON_GetObjectItem(data, "lux");

    if (cJSON_IsNumber(temp) && cJSON_IsNumber(humi))
    {
        msg->is_dht11 = true;
        msg->data.dht11.temp = temp->valueint;
        msg->data.dht11.humi = humi->valueint;
    }
    else if (cJSON_IsNumber(lux))
    {
        msg->is_dht11 = false;
        msg->data.lux_sensor.lux = lux->valueint;
    }
    else
    {
        goto error;
    }

    cJSON_Delete(root);
    return msg;

error:
    free(msg);
exit:
    cJSON_Delete(root);
    return NULL;
}

// --- Encode Slave Message Data for MQTT ---
char *json_encode_slave_data_for_mqtt(const json_slave_msg_t *msg)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    cJSON *data_obj = cJSON_CreateObject();
    if (msg->is_dht11)
    {
        cJSON_AddNumberToObject(data_obj, "temp", msg->data.dht11.temp);
        cJSON_AddNumberToObject(data_obj, "humi", msg->data.dht11.humi);
    }
    else
    {
        cJSON_AddNumberToObject(data_obj, "lux", msg->data.lux_sensor.lux);
    }
    cJSON_AddItemToObject(root, "data", data_obj);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}
