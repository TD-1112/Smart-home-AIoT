#include "message.h"

uint16_t create_message(Type_Message type_mess, uint16_t value, uint8_t *data_out)
{
    uint16_t len = 0;

    // ---- Start frame ----
    data_out[len++] = 0xAA;
    data_out[len++] = 0x55;

    // ---- Type ----
    data_out[len++] = (uint8_t)type_mess;

    data_out[len++] = 0x00; // length low
    data_out[len++] = 0x00; // length high

    if (type_mess == RESPONSE_MESSAGE)
    {
        uint8_t *val = math.convert.uint16_to_bytes(value);
        data_out[len++] = val[0];
        data_out[len++] = val[1];
    }

    uint16_t length_total = len + 2; // +2 byte checksum
    data_out[4] = (uint8_t)(length_total >> 8);
    data_out[3] = (uint8_t)(length_total & 0xFF);

    uint16_t checksum = caculate_checksum(data_out, len);
    uint8_t *chk = math.convert.uint16_to_bytes(checksum);
    data_out[len++] = chk[0];
    data_out[len++] = chk[1];

    return len;
}

/**
 * @brief checksum 16-bit
 */
uint16_t caculate_checksum(uint8_t *data, uint16_t length_data)
{
    uint32_t sum = 0;
    for (uint16_t i = 0; i < length_data; i++)
        sum += data[i];
    return (uint16_t)(sum & 0xFFFF);
}
