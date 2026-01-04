#ifndef __LIB_MATH__
#define __LIB_MATH__

#include <stdint.h>

typedef union {
    float data_float;
    uint8_t data_bytes[4];
} data_convert_float_and_bytes;

typedef union {
    uint16_t data_uint16;
    uint8_t data_bytes[2];
} data_convert_uint16_and_bytes;

typedef struct {
    struct {
        uint8_t (*add)(uint8_t, uint8_t);
        uint8_t (*sub)(uint8_t, uint8_t);
        long long (*mul)(uint8_t, uint8_t);
        uint8_t (*div)(uint8_t, uint8_t);
        uint8_t (*max)(uint8_t, uint8_t);
        uint8_t (*min)(uint8_t, uint8_t);
    } u8;

    struct {
        uint16_t (*add)(uint16_t, uint16_t);
        uint16_t (*sub)(uint16_t, uint16_t);
        long long (*mul)(uint16_t, uint16_t);
        uint16_t (*div)(uint16_t, uint16_t);
        uint16_t (*max)(uint16_t, uint16_t);
        uint16_t (*min)(uint16_t, uint16_t);
    } u16;

    struct {
        uint32_t (*add)(uint32_t, uint32_t);
        uint32_t (*sub)(uint32_t, uint32_t);
        long long (*mul)(uint32_t, uint32_t);
        uint32_t (*div)(uint32_t, uint32_t);
        uint32_t (*max)(uint32_t, uint32_t);
        uint32_t (*min)(uint32_t, uint32_t);
    } u32;

    struct {
        uint8_t*  (*float_to_bytes)(float);
        uint8_t*  (*uint16_to_bytes)(uint16_t);
        float     (*bytes_to_float)(uint8_t, uint8_t, uint8_t, uint8_t);
        uint16_t  (*bytes_to_uint16)(uint8_t, uint8_t);
    } convert;

} math_t;

extern const math_t math;

#endif
