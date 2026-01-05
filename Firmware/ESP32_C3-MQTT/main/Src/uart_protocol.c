#include "uart_protocol.h"
#include "message.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "UART_PROTOCOL";

// ============ JSON TO UART ============

void extract_sensor_data_from_json(const char *json_str, Sensor_Data *sensor_data)
{
    if (json_str == NULL || sensor_data == NULL)
    {
        return;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL)
    {
        ESP_LOGW(TAG, "Failed to parse JSON");
        return;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data != NULL && cJSON_IsObject(data))
    {
        // get lux value if exists
        cJSON *lux = cJSON_GetObjectItem(data, "lux");
        if (lux != NULL && cJSON_IsNumber(lux))
        {
            sensor_data->lux = (uint16_t)lux->valueint;
            sensor_data->flags |= SENSOR_FLAG_LUX;
            ESP_LOGI(TAG, "Found lux: %d", sensor_data->lux);
        }

        // get temp value if exists
        cJSON *temp = cJSON_GetObjectItem(data, "temp");
        if (temp != NULL && cJSON_IsNumber(temp))
        {
            sensor_data->temp = (uint8_t)temp->valueint;
            sensor_data->flags |= SENSOR_FLAG_TEMP;
            ESP_LOGI(TAG, "Found temp: %d", sensor_data->temp);
        }

        // get humi value if exists
        cJSON *humi = cJSON_GetObjectItem(data, "humi");
        if (humi != NULL && cJSON_IsNumber(humi))
        {
            sensor_data->humi = (uint8_t)humi->valueint;
            sensor_data->flags |= SENSOR_FLAG_HUMI;
            ESP_LOGI(TAG, "Found humi: %d", sensor_data->humi);
        }
    }

    cJSON_Delete(root);
}

/**
 * @brief Extract sensor data from multiple JSON messages
 *
 * @param json_messages
 * @param count
 * @param sensor_data
 */
void extract_sensor_data_from_multiple_json(const char **json_messages, int count, Sensor_Data *sensor_data)
{
    // initialize sensor_data fields
    sensor_data->flags = SENSOR_FLAG_NONE;
    sensor_data->lux = 0;
    sensor_data->temp = 0;
    sensor_data->humi = 0;

    if (json_messages == NULL || count == 0 || sensor_data == NULL)
    {
        return;
    }

    // Iterate through each JSON message and extract data
    for (int i = 0; i < count; i++)
    {
        if (json_messages[i] != NULL)
        {
            extract_sensor_data_from_json(json_messages[i], sensor_data);
        }
    }

    ESP_LOGI(TAG, "Final sensor data - flags: 0x%02X, lux: %d, temp: %d, humi: %d",
             sensor_data->flags, sensor_data->lux, sensor_data->temp, sensor_data->humi);
}

/**
 * @brief Create a uart data message object
 * @param sensor_data -> sensor data to encode
 * @param data_out -> array to store uart data message
 * @return uint16_t  - length of data_out
 */
uint16_t create_uart_data_message(Sensor_Data *sensor_data, uint8_t *data_out)
{
    if (data_out == NULL || sensor_data == NULL)
    {
        ESP_LOGE(TAG, "Invalid parameters");
        return 0;
    }

    uint16_t idx = 0;

    // Start bytes
    data_out[idx++] = 0xAA;
    data_out[idx++] = 0x55;

    // Type message
    data_out[idx++] = UART_MSG_DATA;

    // Length placeholder (will fill later)
    uint16_t length_pos = idx;
    data_out[idx++] = 0x00; // length high
    data_out[idx++] = 0x00; // length low

    // Data payload: [flags, lux_high, lux_low, temp, humi]
    data_out[idx++] = sensor_data->flags;
    data_out[idx++] = (sensor_data->lux >> 8) & 0xFF;
    data_out[idx++] = sensor_data->lux & 0xFF;
    data_out[idx++] = sensor_data->temp;
    data_out[idx++] = sensor_data->humi;

    // Calculate total length (from 0xAA to end of data, excluding checksum)
    uint16_t total_length = idx + 2; // +2 for checksum

    // Write length to the correct position (little endian - bytes_to_uint16 uses union)
    data_out[length_pos] = (uint8_t)(total_length & 0xFF);   // low byte
    data_out[length_pos + 1] = (uint8_t)(total_length >> 8); // high byte

    // Calculate checksum (all bytes before checksum)
    uint16_t checksum = caculate_checksum(data_out, idx);

    // Add checksum (little endian like uint16_to_bytes)
    uint8_t *chk = math.convert.uint16_to_bytes(checksum);
    data_out[idx++] = chk[0];
    data_out[idx++] = chk[1];

    ESP_LOGI(TAG, "Created UART data message: flags=0x%02X, lux=%d, temp=%d, humi=%d",
             sensor_data->flags, sensor_data->lux, sensor_data->temp, sensor_data->humi);

    return idx; // Return total number of bytes written
}

/**
 * @brief Parse JSON string to UART data message
 * @param json_str -> input JSON string
 * @param data_out -> output UART data message
 * @return uint16_t -> length of data_out
 */
uint16_t parse_json_to_uart_data(const char *json_str, uint8_t *data_out)
{
    if (data_out == NULL)
    {
        ESP_LOGE(TAG, "data_out is NULL");
        return 0;
    }

    Sensor_Data sensor_data = {.flags = SENSOR_FLAG_NONE, .lux = 0, .temp = 0, .humi = 0};

    // Parse JSON nếu có
    if (json_str != NULL)
    {
        extract_sensor_data_from_json(json_str, &sensor_data);
    }

    return create_uart_data_message(&sensor_data, data_out);
}

/**
 * @brief Create a uart control message object`
 *
 * @param plug_id
 * @param status
 * @param data_out
 * @return uint16_t
 */
uint16_t create_uart_control_message(Plug_ID plug_id, Plug_Status status, uint8_t *data_out)
{
    if (data_out == NULL)
    {
        ESP_LOGE(TAG, "data_out is NULL");
        return 0;
    }

    uint16_t idx = 0;

    // Start bytes
    data_out[idx++] = 0xAA;
    data_out[idx++] = 0x55;

    // Type message
    data_out[idx++] = UART_MSG_CONTROL;

    // Length placeholder
    uint16_t length_pos = idx;
    data_out[idx++] = 0x00; // length high
    data_out[idx++] = 0x00; // length low

    // Data payload: [plug_id, status]
    data_out[idx++] = (uint8_t)plug_id;
    data_out[idx++] = (uint8_t)status;

    // Calculate total length
    uint16_t total_length = idx + 2; // +2 for checksum

    // Write length (little endian - bytes_to_uint16 uses union)
    data_out[length_pos] = (uint8_t)(total_length & 0xFF);   // low byte
    data_out[length_pos + 1] = (uint8_t)(total_length >> 8); // high byte

    // Calculate checksum
    uint16_t checksum = caculate_checksum(data_out, idx);
    uint8_t *chk = math.convert.uint16_to_bytes(checksum);
    data_out[idx++] = chk[0];
    data_out[idx++] = chk[1];

    ESP_LOGI(TAG, "Created UART control message: plug=%d, status=%d", plug_id, status);

    return idx;
}

// ============ UART TO JSON ============

/**
 * @brief Decode UART data message to JSON string
 *
 * @param uart_data
 * @param length
 * @return char*
 */
char *decode_uart_data_to_json(const uint8_t *uart_data, uint16_t length)
{
    if (uart_data == NULL || length < sizeof(Frame_Message))
    {
        ESP_LOGE(TAG, "Invalid UART data");
        return NULL;
    }

    Frame_Message *frame = (Frame_Message *)uart_data;

    // Check start message
    if (frame->start_message != 0xAA55)
    {
        ESP_LOGE(TAG, "Invalid start message: 0x%04X", frame->start_message);
        return NULL;
    }

    // Check message type
    if (frame->type_message != UART_MSG_DATA)
    {
        ESP_LOGE(TAG, "Not a data message: 0x%02X", frame->type_message);
        return NULL;
    }

    // Check     length
    if (frame->length_message < 5)
    {
        ESP_LOGE(TAG, "Invalid data length: %d", frame->length_message);
        return NULL;
    }

    // Extract data
    uint8_t flags = frame->data[0];
    uint16_t lux = (frame->data[1] << 8) | frame->data[2];
    uint8_t temp = frame->data[3];
    uint8_t humi = frame->data[4];

    // Create JSON telemetry
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "telemetry");

    cJSON *data = cJSON_CreateObject();

    // Add sensor values based on flags
    if (flags & SENSOR_FLAG_LUX)
    {
        cJSON_AddNumberToObject(data, "lux", lux);
    }

    if (flags & SENSOR_FLAG_TEMP)
    {
        cJSON_AddNumberToObject(data, "temp", temp);
    }

    if (flags & SENSOR_FLAG_HUMI)
    {
        cJSON_AddNumberToObject(data, "humi", humi);
    }

    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Decoded UART data to JSON (flags=0x%02X): %s", flags, json_str);

    return json_str;
}

/**
 * @brief Decode UART control message to JSON string
 *
 * @param uart_data
 * @param length
 * @return char*
 */
char *decode_uart_control_to_json(const uint8_t *uart_data, uint16_t length)
{
    if (uart_data == NULL || length < sizeof(Frame_Message))
    {
        ESP_LOGE(TAG, "Invalid UART data");
        return NULL;
    }

    Frame_Message *frame = (Frame_Message *)uart_data;

    // Check start message
    if (frame->start_message != 0xAA55)
    {
        ESP_LOGE(TAG, "Invalid start message: 0x%04X", frame->start_message);
        return NULL;
    }

    // Check message type
    if (frame->type_message != UART_MSG_CONTROL)
    {
        ESP_LOGE(TAG, "Not a control message: 0x%02X", frame->type_message);
        return NULL;
    }

    // Check length
    if (frame->length_message < 2)
    {
        ESP_LOGE(TAG, "Invalid data length: %d", frame->length_message);
        return NULL;
    }

    // Extract data
    uint8_t plug_id = frame->data[0];
    uint8_t status = frame->data[1];

    // Create JSON control
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "control");

    cJSON *data = cJSON_CreateObject();

    // Create plug name
    char plug_name[10];
    snprintf(plug_name, sizeof(plug_name), "plug_%d", plug_id + 1);

    cJSON_AddStringToObject(data, "plug", plug_name);
    cJSON_AddStringToObject(data, "status", status == STATUS_ON ? "on" : "off");

    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Decoded UART control to JSON: %s", json_str);

    return json_str;
}
