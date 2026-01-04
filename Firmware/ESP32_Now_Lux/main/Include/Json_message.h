#ifndef JSON_MESSAGE_H
#define JSON_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "cJSON.h"

#define SLAVE_NAME_LEN 32
#define MAC_STR_LEN 18

// Defines the type of JSON message
typedef enum
{
    JSON_MSG_TYPE_DISCOVERY,
    JSON_MSG_TYPE_DISCOVERY_RESPONSE,
    JSON_MSG_TYPE_ASK_DATA,
    JSON_MSG_TYPE_RESPONSE_DATA,
    JSON_MSG_TYPE_CONTROL, // For control commands from master
    JSON_MSG_TYPE_UNKNOWN
} json_msg_type_t;

// Defines control commands from the master
typedef enum
{
    JSON_CMD_REGISTER_SUCCESS,
    // Add other commands here in the future
} json_cmd_t;

/* --- Master -> Slave Message Structure --- */

// Represents a message received from the master
typedef struct
{
    json_msg_type_t type;
    char id[MAC_STR_LEN];
    // 'dst' is optional and may not always be present

    // Fields for control messages
    bool has_cmd;
    json_cmd_t cmd;
} json_master_msg_t;

/* --- Slave -> Master Message Structures --- */

// For responding to discovery requests
typedef struct
{
    json_msg_type_t type; // Should be JSON_MSG_TYPE_DISCOVERY_RESPONSE
    char id[MAC_STR_LEN];
    char name[SLAVE_NAME_LEN];
} json_slave_disc_resp_t;

// For responding to data requests
typedef struct
{
    json_msg_type_t type; // Should be JSON_MSG_TYPE_RESPONSE_DATA
    char id[MAC_STR_LEN];
    char dst[MAC_STR_LEN];
    struct
    {
        int temp;
        int humi;
        int lux;
    } data;
} json_slave_data_t;

/* --- Function Prototypes --- */

/**
 * @brief Decodes a JSON string into a master message structure.
 *
 * @param json_str The JSON string received from the master.
 * @return A pointer to a dynamically allocated json_master_msg_t structure, or NULL on failure.
 *         The caller is responsible for freeing this memory.
 */
json_master_msg_t *json_decode_master_msg(const char *json_str);

/**
 * @brief Encodes a slave discovery response structure into a JSON string.
 *
 * @param disc_resp Pointer to the discovery response structure.
 * @return A dynamically allocated JSON string, or NULL on failure.
 *         The caller is responsible for freeing this memory.
 */
char *json_encode_slave_discovery_response(const json_slave_disc_resp_t *disc_resp);

/**
 * @brief Encodes a slave data response structure into a JSON string.
 *
 * @param data_resp Pointer to the data response structure.
 * @return A dynamically allocated JSON string, or NULL on failure.
 *         The caller is responsible for freeing this memory.
 */
char *json_encode_slave_data(const json_slave_data_t *data_resp);

#endif // JSON_MESSAGE_H