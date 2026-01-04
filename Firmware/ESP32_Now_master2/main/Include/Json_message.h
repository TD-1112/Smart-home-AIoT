#ifndef JSON_MESSAGE_H
#define JSON_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <cJSON.h>

#define MAC_ADDR_STR_LEN 18
#define SLAVE_NAME_LEN 32

// Command types
typedef enum
{
    JSON_MSG_TYPE_DISCOVERY = 0,
    JSON_MSG_TYPE_DISCOVERY_RESPONSE,
    JSON_MSG_TYPE_ASK_DATA,
    JSON_MSG_TYPE_CONTROL,
    JSON_MSG_TYPE_RESPONSE_DATA,
    JSON_MSG_TYPE_UNKNOWN
} json_msg_type_t;

// Control command types
typedef enum
{
    JSON_CMD_TURN_ON_LED = 0,
    JSON_CMD_TURN_OFF_LED,
    JSON_CMD_REGISTER_SUCCESS
} json_cmd_type_t;

// Message structure for master to slave
typedef struct
{
    char id[MAC_ADDR_STR_LEN];  // Master MAC
    char dst[MAC_ADDR_STR_LEN]; // Slave MAC or "broadcast"
    json_msg_type_t type;
    json_cmd_type_t cmd; // Only for control
    bool has_cmd;        // true if cmd is valid
} json_master_msg_t;

// Message structure for slave's discovery response
typedef struct
{
    char id[MAC_ADDR_STR_LEN];   // Slave MAC
    char name[SLAVE_NAME_LEN]; // Slave name
} json_discovery_response_t;

// Message structure for slave to master (response)
typedef struct
{
    char id[MAC_ADDR_STR_LEN];  // Slave MAC
    char dst[MAC_ADDR_STR_LEN]; // Master MAC
    json_msg_type_t type;       // Always RESPONSE_DATA
    union
    {
        struct
        {
            int temp;
            int humi;
        } dht11;
        struct
        {
            int lux;
        } lux_sensor;
    } data;
    bool is_dht11; // true if DHT11, false if lux
} json_slave_msg_t;

// --- API ---

// Get the type of the message from a JSON string
json_msg_type_t json_decode_msg_type(const char *json_str);

// Encode master message to JSON string
char *json_encode_master_msg(const json_master_msg_t *msg);

// Decode master message from JSON string, return pointer to struct or NULL if error
json_master_msg_t *json_decode_master_msg(const char *json_str);

// Decode discovery response from JSON string
json_discovery_response_t *json_decode_discovery_response(const char *json_str);

// Encode slave message to JSON string (response)
char *json_encode_slave_msg(const json_slave_msg_t *msg);

// Decode slave message from JSON string, return pointer to struct or NULL if error
json_slave_msg_t *json_decode_slave_msg(const char *json_str);

// Encode slave message data to JSON string for MQTT
char *json_encode_slave_data_for_mqtt(const json_slave_msg_t *msg);

#endif // JSON_MESSAGE_H
