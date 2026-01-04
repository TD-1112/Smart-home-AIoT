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
        // Lấy lux nếu có
        cJSON *lux = cJSON_GetObjectItem(data, "lux");
        if (lux != NULL && cJSON_IsNumber(lux))
        {
            sensor_data->lux = (uint16_t)lux->valueint;
            sensor_data->flags |= SENSOR_FLAG_LUX;
            ESP_LOGI(TAG, "Found lux: %d", sensor_data->lux);
        }

        // Lấy temp nếu có
        cJSON *temp = cJSON_GetObjectItem(data, "temp");
        if (temp != NULL && cJSON_IsNumber(temp))
        {
            sensor_data->temp = (uint8_t)temp->valueint;
            sensor_data->flags |= SENSOR_FLAG_TEMP;
            ESP_LOGI(TAG, "Found temp: %d", sensor_data->temp);
        }

        // Lấy humi nếu có
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

void extract_sensor_data_from_multiple_json(const char **json_messages, int count, Sensor_Data *sensor_data)
{
    // Khởi tạo tất cả giá trị = 0 và flags = NONE
    sensor_data->flags = SENSOR_FLAG_NONE;
    sensor_data->lux = 0;
    sensor_data->temp = 0;
    sensor_data->humi = 0;

    if (json_messages == NULL || count == 0 || sensor_data == NULL)
    {
        return;
    }

    // Duyệt qua tất cả các JSON messages
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

    // Length placeholder (sẽ điền sau)
    uint16_t length_pos = idx;
    data_out[idx++] = 0x00; // length low
    data_out[idx++] = 0x00; // length high

    // Data payload: [flags, lux_high, lux_low, temp, humi]
    data_out[idx++] = sensor_data->flags;
    data_out[idx++] = (sensor_data->lux >> 8) & 0xFF;
    data_out[idx++] = sensor_data->lux & 0xFF;
    data_out[idx++] = sensor_data->temp;
    data_out[idx++] = sensor_data->humi;

    // Tính tổng độ dài (từ 0xAA đến cuối data, chưa tính checksum)
    uint16_t total_length = idx + 2; // +2 cho checksum

    // Ghi length vào đúng vị trí (little endian - bytes_to_uint16 dùng union)
    data_out[length_pos] = (uint8_t)(total_length & 0xFF);   // low byte
    data_out[length_pos + 1] = (uint8_t)(total_length >> 8); // high byte

    // Tính checksum (tất cả bytes trước checksum)
    uint16_t checksum = caculate_checksum(data_out, idx);

    // Thêm checksum (little endian như uint16_to_bytes)
    uint8_t *chk = math.convert.uint16_to_bytes(checksum);
    data_out[idx++] = chk[0];
    data_out[idx++] = chk[1];

    ESP_LOGI(TAG, "Created UART data message: flags=0x%02X, lux=%d, temp=%d, humi=%d",
             sensor_data->flags, sensor_data->lux, sensor_data->temp, sensor_data->humi);

    return idx; // Trả về tổng số byte đã ghi
}

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
    data_out[idx++] = 0x00; // length low
    data_out[idx++] = 0x00; // length high

    // Data payload: [plug_id, status]
    data_out[idx++] = (uint8_t)plug_id;
    data_out[idx++] = (uint8_t)status;

    // Tính tổng độ dài
    uint16_t total_length = idx + 2; // +2 cho checksum

    // Ghi length (little endian - bytes_to_uint16 dùng union)
    data_out[length_pos] = (uint8_t)(total_length & 0xFF);   // low byte
    data_out[length_pos + 1] = (uint8_t)(total_length >> 8); // high byte

    // Tính checksum
    uint16_t checksum = caculate_checksum(data_out, idx);
    uint8_t *chk = math.convert.uint16_to_bytes(checksum);
    data_out[idx++] = chk[0];
    data_out[idx++] = chk[1];

    ESP_LOGI(TAG, "Created UART control message: plug=%d, status=%d", plug_id, status);

    return idx;
}

// ============ UART TO JSON ============

static bool uart_parse_and_check_frame(const uint8_t *uart_data, uint16_t buf_len, uint8_t expected_type,
                                       const uint8_t **payload_out, uint16_t *payload_len_out)
{
    if (!uart_data || buf_len < 7)
    {
        return false;
    }

    if (uart_data[0] != 0xAA || uart_data[1] != 0x55)
    {
        return false;
    }

    if (uart_data[2] != expected_type)
    {
        return false;
    }

    // Length is little-endian: [3]=low, [4]=high
    uint16_t frame_len = math.convert.bytes_to_uint16(uart_data[3], uart_data[4]);
    if (frame_len < 7 || frame_len > buf_len)
    {
        return false;
    }

    // checksum is last 2 bytes, little-endian
    uint16_t received_chk = math.convert.bytes_to_uint16(uart_data[frame_len - 2], uart_data[frame_len - 1]);
    uint16_t calc_chk = caculate_checksum((uint8_t *)uart_data, (uint16_t)(frame_len - 2));
    if (received_chk != calc_chk)
    {
        ESP_LOGE(TAG, "Checksum mismatch: recv=0x%04X calc=0x%04X", received_chk, calc_chk);
        return false;
    }

    const uint8_t *payload = &uart_data[5];
    uint16_t payload_len = (uint16_t)(frame_len - 5 - 2);

    if (payload_out)
        *payload_out = payload;
    if (payload_len_out)
        *payload_len_out = payload_len;

    return true;
}

char *decode_uart_data_to_json(const uint8_t *uart_data, uint16_t length)
{
    const uint8_t *payload = NULL;
    uint16_t payload_len = 0;
    if (!uart_parse_and_check_frame(uart_data, length, UART_MSG_DATA, &payload, &payload_len))
    {
        ESP_LOGE(TAG, "Invalid UART DATA frame");
        return NULL;
    }

    if (payload_len < 5)
    {
        ESP_LOGE(TAG, "Invalid DATA payload length: %u", (unsigned)payload_len);
        return NULL;
    }

    uint8_t flags = payload[0];
    uint16_t lux = ((uint16_t)payload[1] << 8) | payload[2];
    uint8_t temp = payload[3];
    uint8_t humi = payload[4];

    // Tạo JSON telemetry
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "telemetry");

    cJSON *data = cJSON_CreateObject();

    // Chỉ thêm các giá trị có flag tương ứng
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

char *decode_uart_control_to_json(const uint8_t *uart_data, uint16_t length)
{
    const uint8_t *payload = NULL;
    uint16_t payload_len = 0;
    if (!uart_parse_and_check_frame(uart_data, length, UART_MSG_CONTROL, &payload, &payload_len))
    {
        ESP_LOGE(TAG, "Invalid UART CONTROL frame");
        return NULL;
    }

    if (payload_len < 2)
    {
        ESP_LOGE(TAG, "Invalid CONTROL payload length: %u", (unsigned)payload_len);
        return NULL;
    }

    uint8_t plug_id = payload[0];
    uint8_t status = payload[1];

    // Tạo JSON control
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "control");

    cJSON *data = cJSON_CreateObject();

    // Tạo plug name
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
