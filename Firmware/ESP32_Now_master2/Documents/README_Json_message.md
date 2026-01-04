# ESP-NOW JSON Message Library

This library provides encoding and decoding functions for ESP-NOW JSON messages between master and slave ESP32 devices, as described in the project specification.

## Features
- Encode/decode master-to-slave messages (ask_data, control)
- Encode/decode slave-to-master response messages (DHT11 or lux sensor)
- Easy to reuse for other ESP32 nodes

## Usage

### 1. Include the header
```c
#include "Json_message.h"
```

### 2. Encode a master message (ask_data)
```c
json_master_msg_t msg = {
    .id = "AA:BB:CC:DD:EE:FF",
    .dst = "11:22:33:44:55:66",
    .type = JSON_MSG_ASK_DATA,
    .has_cmd = false
};
char *json_str = json_encode_master_msg(&msg);
// send json_str ...
free(json_str);
```

### 3. Encode a master message (control)
```c
json_master_msg_t msg = {
    .id = "AA:BB:CC:DD:EE:FF",
    .dst = "11:22:33:44:55:66",
    .type = JSON_MSG_CONTROL,
    .cmd = JSON_CMD_TURN_ON_LED,
    .has_cmd = true
};
char *json_str = json_encode_master_msg(&msg);
// send json_str ...
free(json_str);
```

### 4. Decode a master message
```c
json_master_msg_t msg;
if (json_decode_master_msg(json_str, &msg)) {
    // use msg
}
```

### 5. Encode a slave response (DHT11)
```c
json_slave_msg_t resp = {
    .id = "11:22:33:44:55:66",
    .dst = "AA:BB:CC:DD:EE:FF",
    .type = JSON_MSG_RESPONSE_DATA,
    .is_dht11 = true,
    .data.dht11 = { .temp = 25, .humi = 80 }
};
char *json_str = json_encode_slave_msg(&resp);
free(json_str);
```

### 6. Encode a slave response (lux)
```c
json_slave_msg_t resp = {
    .id = "11:22:33:44:55:66",
    .dst = "AA:BB:CC:DD:EE:FF",
    .type = JSON_MSG_RESPONSE_DATA,
    .is_dht11 = false,
    .data.lux_sensor = { .lux = 1234142 }
};
char *json_str = json_encode_slave_msg(&resp);
free(json_str);
```

### 7. Decode a slave response
```c
json_slave_msg_t resp;
if (json_decode_slave_msg(json_str, &resp)) {
    if (resp.is_dht11) {
        // use resp.data.dht11.temp, resp.data.dht11.humi
    } else {
        // use resp.data.lux_sensor.lux
    }
}
```

## API
See Json_message.h for full API details.

## Dependencies
- cJSON library

## File List
- components/esp_now/Json_message.h
- components/esp_now/Json_message.c
