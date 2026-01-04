#include "lib_math.h"

// ==== u8 ====
uint8_t add_u8(uint8_t a, uint8_t b) { return a + b; }
uint8_t sub_u8(uint8_t a, uint8_t b) { return a - b; }
long long mul_u8(uint8_t a, uint8_t b) { return a * b; }
uint8_t div_u8(uint8_t a, uint8_t b) { return (b == 0) ? 0 : a / b; }
uint8_t max_u8(uint8_t a, uint8_t b) { return (a > b) ? a : b; }
uint8_t min_u8(uint8_t a, uint8_t b) { return (a < b) ? a : b; }

// ==== u16 ====
uint16_t add_u16(uint16_t a, uint16_t b) { return a + b; }
uint16_t sub_u16(uint16_t a, uint16_t b) { return a - b; }
long long mul_u16(uint16_t a, uint16_t b) { return a * b; }
uint16_t div_u16(uint16_t a, uint16_t b) { return (b == 0) ? 0 : a / b; }
uint16_t max_u16(uint16_t a, uint16_t b) { return (a > b) ? a : b; }
uint16_t min_u16(uint16_t a, uint16_t b) { return (a < b) ? a : b; }

// ==== u32 ====
uint32_t add_u32(uint32_t a, uint32_t b) { return a + b; }
uint32_t sub_u32(uint32_t a, uint32_t b) { return a - b; }
long long mul_u32(uint32_t a, uint32_t b) { return a * b; }
uint32_t div_u32(uint32_t a, uint32_t b) { return (b == 0) ? 0 : a / b; }
uint32_t max_u32(uint32_t a, uint32_t b) { return (a > b) ? a : b; }
uint32_t min_u32(uint32_t a, uint32_t b) { return (a < b) ? a : b; }

// ==== convert ====
uint8_t* float_to_bytes(float value) {
    static data_convert_float_and_bytes d;
    d.data_float = value;
    return d.data_bytes;
}

uint8_t* uint16_to_bytes(uint16_t value) {
    static data_convert_uint16_and_bytes d;
    d.data_uint16 = value;
    return d.data_bytes;
}

float bytes_to_float(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    data_convert_float_and_bytes d;
    d.data_bytes[0] = b1;
    d.data_bytes[1] = b2;
    d.data_bytes[2] = b3;
    d.data_bytes[3] = b4;
    return d.data_float;
}

uint16_t bytes_to_uint16(uint8_t b1, uint8_t b2) {
    data_convert_uint16_and_bytes d;
    d.data_bytes[0] = b1;
    d.data_bytes[1] = b2;
    return d.data_uint16;
}

// ==== Init math ====
const math_t math = {
    .u8 = {
        .add = add_u8,
        .sub = sub_u8,
        .mul = mul_u8,
        .div = div_u8,
        .max = max_u8,
        .min = min_u8,
    },
    .u16 = {
        .add = add_u16,
        .sub = sub_u16,
        .mul = mul_u16,
        .div = div_u16,
        .max = max_u16,
        .min = min_u16,
    },
    .u32 = {
        .add = add_u32,
        .sub = sub_u32,
        .mul = mul_u32,
        .div = div_u32,
        .max = max_u32,
        .min = min_u32,
    },
    .convert = {
        .float_to_bytes = float_to_bytes,
        .uint16_to_bytes = uint16_to_bytes,
        .bytes_to_float = bytes_to_float,
        .bytes_to_uint16 = bytes_to_uint16,
    }
};
