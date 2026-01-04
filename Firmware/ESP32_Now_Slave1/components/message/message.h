#ifndef __MESSAGE__
#define __MESSAGE__

#include <stdint.h>
#include "lib_math.h"

typedef struct
{
    uint16_t start_message;  // start frame is 0xAA55
    uint16_t length_message; // length of message
    uint8_t data[100];
    uint8_t type_message;
    uint16_t check_sum;
} Frame_Message;

typedef enum
{
    ASK_MESSAGE = 0x00,
    RESPONSE_MESSAGE = 0x01
} Type_Message;

uint16_t create_message(Type_Message type_mess, uint16_t value, uint8_t *data_out);
uint16_t caculate_checksum(uint8_t *data, uint16_t length_data);

#endif