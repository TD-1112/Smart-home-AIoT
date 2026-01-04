# API Layer Usage Guide

## Overview
The ESP-NOW communication logic has been refactored into a clean API layer (`api.h` / `api.c`) to keep `main.c` minimal and focused on application logic. The API provides both send capabilities and an optional queue-based receive mechanism.

## Core Send Functions

### `void espnow_api_init(const uint8_t default_peer_mac[6])`
Initializes the ESP-NOW system with an optional default peer.

**Parameters:**
- `default_peer_mac`: optional 6-byte MAC address of the peer device (pass NULL to skip)

**Behavior:**
- Initializes ESP-NOW with default config (no encryption, channel 0)
- Registers internal logging callbacks for data receive and send status
- If MAC provided, adds the peer to the ESP-NOW peer list
- Creates a default receive queue (can be customized with `espnow_api_recv_queue_init()`)

**Example:**
```c
static const uint8_t SLAVE_MAC[6] = {0xEC, 0xDA, 0x3B, 0xBF, 0xA0, 0x04};
espnow_api_init(SLAVE_MAC);
```

---

### `esp_err_t espnow_api_send(const uint8_t *data, size_t len)`
Sends raw bytes to the configured default peer.

**Parameters:**
- `data`: Pointer to data buffer
- `len`: Number of bytes to send

**Returns:**
- `ESP_OK` on success
- `ESP_ERR_INVALID_STATE` if no default peer configured
- `ESP_ERR_INVALID_ARG` if data is NULL or len is 0
- Other ESP-NOW error codes on failure

**Example:**
```c
const char *msg = "hello";
esp_err_t ret = espnow_api_send((const uint8_t *)msg, strlen(msg));
if (ret != ESP_OK) {
    ESP_LOGE("APP", "Send failed: %s", esp_err_to_name(ret));
}
```

---

### `esp_err_t espnow_api_send_str(const char *msg)`
Convenience wrapper to send a null-terminated string to the default peer.

**Parameters:**
- `msg`: Pointer to null-terminated string

**Returns:**
- Same as `espnow_api_send()`

**Example:**
```c
espnow_api_send_str("hello from ESP32");
```

---

### `esp_err_t espnow_api_send_to(const uint8_t *peer_mac, const uint8_t *data, size_t len)`
Sends raw bytes to an arbitrary peer MAC address.

**Parameters:**
- `peer_mac`: 6-byte MAC address of target peer
- `data`: Pointer to data buffer
- `len`: Number of bytes to send

**Returns:**
- `ESP_OK` on success
- Error codes otherwise

**Example:**
```c
uint8_t custom_peer[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
espnow_api_send_to(custom_peer, (const uint8_t *)"msg", 3);
```

---

## Receive Functions (Queue-Based)

### `esp_err_t espnow_api_recv_queue_init(size_t queue_len)`
Initializes (or reinitializes) the receive queue with a custom length.

**Parameters:**
- `queue_len`: Maximum number of messages to buffer

**Returns:**
- `ESP_OK` on success, or error code

**Note:** A default queue is created automatically by `espnow_api_init()`. Call this if you need a different queue size.

**Example:**
```c
espnow_api_recv_queue_init(16);  // Buffer up to 16 messages
```

---

### `esp_err_t espnow_api_recv(espnow_msg_t *out_msg, TickType_t ticks_to_wait)`
Pop a received message from the API queue.

**Parameters:**
- `out_msg`: Pointer to `espnow_msg_t` struct to receive the message
- `ticks_to_wait`: FreeRTOS ticks to block waiting for a message (use `portMAX_DELAY` for infinite)

**Returns:**
- `ESP_OK` when a message was retrieved
- `ESP_ERR_TIMEOUT` if timed out waiting
- Other `esp_err_t` on error

**Message Structure:**
```c
typedef struct {
    uint8_t src_mac[6];      // Sender's MAC address
    uint8_t data[256];       // Message payload (up to 256 bytes)
    int len;                 // Actual message length
} espnow_msg_t;
```

**Example:**
```c
espnow_msg_t msg;
esp_err_t ret = espnow_api_recv(&msg, pdMS_TO_TICKS(1000));
if (ret == ESP_OK) {
    ESP_LOGI("APP", "Received %d bytes from [%02X:%02X:%02X:%02X:%02X:%02X]",
             msg.len, msg.src_mac[0], msg.src_mac[1], msg.src_mac[2],
             msg.src_mac[3], msg.src_mac[4], msg.src_mac[5]);
}
```

---

## Logging Output
The API provides built-in logging for all ESP-NOW events:

- **Receive events:** Logged as `I (API): Recv from [MAC] => 'data'`
- **Send status:** Logged as `I (API): Sent to [MAC] => SUCCESS/FAIL`

Received data is safely truncated to 255 characters if needed.

---

## Typical Usage Pattern

### Send-Only Application
```c
void app_main(void) {
    const uint8_t SLAVE_MAC[6] = {0xEC, 0xDA, 0x3B, 0xBF, 0xA0, 0x04};
    espnow_api_init(SLAVE_MAC);
    
    while (true) {
        espnow_api_send_str("hello");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
```

### Bidirectional Application
```c
void app_main(void) {
    const uint8_t SLAVE_MAC[6] = {0xEC, 0xDA, 0x3B, 0xBF, 0xA0, 0x04};
    espnow_api_init(SLAVE_MAC);
    espnow_api_recv_queue_init(10);
    
    // Sender task
    xTaskCreate(sender_task, "sender", 2048, NULL, 5, NULL);
    
    // Receiver task
    xTaskCreate(receiver_task, "receiver", 2048, NULL, 5, NULL);
}

void sender_task(void *arg) {
    while (true) {
        espnow_api_send_str("hello");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void receiver_task(void *arg) {
    espnow_msg_t msg;
    while (true) {
        if (espnow_api_recv(&msg, portMAX_DELAY) == ESP_OK) {
            ESP_LOGI("RX", "Got %d bytes", msg.len);
        }
    }
}
```

---

## Extending the API
To add more features (e.g., channel config, encryption, multi-peer support), extend `api.h` with new functions and implement them in `api.c`. This keeps the API contract stable while keeping `main.c` clean.

---

## File Structure
```
main/
├── main.c           ← Application logic only
├── api.h            ← API declarations (public interface)
├── api.c            ← API implementation (private details)
├── CMakeLists.txt   ← Includes both api.c and main.c
└── API_USAGE.md     ← This guide
```

---

## Summary
- **main.c**: Only contains `app_main()` and application-specific constants (like peer MAC)
- **api.c/api.h**: Handles all ESP-NOW initialization, communication, and logging
- **Benefits**: Cleaner main.c, reusable API, easier to test and extend
