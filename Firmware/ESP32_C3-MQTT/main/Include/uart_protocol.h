#ifndef __UART_PROTOCOL_H__
#define __UART_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>
#include "cJSON.h"

// ============ ENUMS ============
typedef enum
{
    PLUG_1 = 0,
    PLUG_2 = 1,
    PLUG_3 = 2
} Plug_ID;

typedef enum
{
    STATUS_OFF = 0,
    STATUS_ON = 1
} Plug_Status;

typedef enum
{
    UART_MSG_DATA = 0x01,   // Bản tin dữ liệu cảm biến
    UART_MSG_CONTROL = 0x02 // Bản tin điều khiển
} UART_Message_Type;

// Cờ để quản lý dữ liệu cảm biến
typedef enum
{
    SENSOR_FLAG_NONE = 0x00, // Không có dữ liệu
    SENSOR_FLAG_LUX = 0x01,  // Có dữ liệu lux
    SENSOR_FLAG_TEMP = 0x02, // Có dữ liệu nhiệt độ
    SENSOR_FLAG_HUMI = 0x04  // Có dữ liệu độ ẩm
} Sensor_Data_Flags;

// ============ STRUCTURES ============
typedef struct
{
    uint8_t flags; // Cờ cho biết dữ liệu nào có sẵn
    uint16_t lux;  // Ánh sáng (0-65535)
    uint8_t temp;  // Nhiệt độ (0-100)
    uint8_t humi;  // Độ ẩm (0-100)
} Sensor_Data;

typedef struct
{
    Plug_ID plug_id;
    Plug_Status status;
} Control_Data;

// ============ JSON TO UART ============
/**
 * @brief Parse JSON và tạo bản tin UART data
 * @param json_str Chuỗi JSON nhận được
 * @param data_out Buffer để lưu bản tin UART
 * @return Độ dài bản tin UART, 0 nếu lỗi
 */
uint16_t parse_json_to_uart_data(const char *json_str, uint8_t *data_out);

/**
 * @brief Tạo bản tin UART control
 * @param plug_id ID công tắc (0-2)
 * @param status Trạng thái (ON/OFF)
 * @param data_out Buffer để lưu bản tin UART
 * @return Độ dài bản tin UART
 */
uint16_t create_uart_control_message(Plug_ID plug_id, Plug_Status status, uint8_t *data_out);

// ============ UART TO JSON ============
/**
 * @brief Decode bản tin UART data và tạo JSON telemetry
 * @param uart_data Dữ liệu UART đã nhận
 * @param length Độ dài dữ liệu
 * @return Chuỗi JSON (cần free sau khi dùng), NULL nếu lỗi
 */
char *decode_uart_data_to_json(const uint8_t *uart_data, uint16_t length);

/**
 * @brief Decode bản tin UART control và tạo JSON control
 * @param uart_data Dữ liệu UART đã nhận
 * @param length Độ dài dữ liệu
 * @return Chuỗi JSON (cần free sau khi dùng), NULL nếu lỗi
 */
char *decode_uart_control_to_json(const uint8_t *uart_data, uint16_t length);

// ============ HELPER FUNCTIONS ============
/**
 * @brief Trích xuất sensor data từ 1 JSON message (sử dụng flags)
 * @param json_str Chuỗi JSON
 * @param sensor_data Output sensor data với flags
 */
void extract_sensor_data_from_json(const char *json_str, Sensor_Data *sensor_data);

/**
 * @brief Trích xuất sensor data từ nhiều JSON messages (sử dụng flags)
 * @param json_messages Mảng các chuỗi JSON
 * @param count Số lượng JSON messages
 * @param sensor_data Output sensor data với flags
 */
void extract_sensor_data_from_multiple_json(const char **json_messages, int count, Sensor_Data *sensor_data);

/**
 * @brief Tạo bản tin UART data từ Sensor_Data
 * @param sensor_data Dữ liệu cảm biến với flags
 * @param data_out Buffer để lưu bản tin UART
 * @return Độ dài bản tin UART
 */
uint16_t create_uart_data_message(Sensor_Data *sensor_data, uint8_t *data_out);

#endif // __UART_PROTOCOL_H__
