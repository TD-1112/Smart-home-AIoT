#ifndef __MESSAGE__
#define __MESSAGE__

#include <stdint.h>
#include "lib_math.h"

typedef struct
{
    uint8_t start_byte;        // 0xAA
    uint8_t start_byte_follow; // 0x55
    uint8_t type_message;      // type
    uint16_t length_message;   // total length (including checksum)
    uint8_t data[100];         // payload (NOT including checksum)
    uint16_t check_sum;        // checksum (little-endian on wire)
} Frame_Message;

typedef enum
{
    ASK_MESSAGE = 0x00,
    RESPONSE_MESSAGE = 0x01
} Type_Message;

uint16_t create_message(Type_Message type_mess, uint16_t value, uint8_t *data_out);
uint16_t caculate_checksum(uint8_t *data, uint16_t length_data);

#endif