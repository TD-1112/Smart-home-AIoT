#ifndef __API__
#define __API__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "my_espnow.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Initialize ESP-NOW helper API, register internal logging callbacks,
    // and set the optional default peer MAC to send to.
    // - default_peer_mac: optional 6-byte MAC address of the peer device (may be NULL)
    void espnow_api_init(const uint8_t default_peer_mac[6]);

    // Send raw bytes to the configured default peer.
    // Returns ESP_OK on success, or appropriate esp_err_t on failure.
    esp_err_t espnow_api_send(const uint8_t *data, size_t len);

    // Convenience helper to send a null-terminated string to the default peer.
    esp_err_t espnow_api_send_str(const char *msg);

    // Send raw bytes to an arbitrary peer MAC (useful for replying to sender)
    esp_err_t espnow_api_send_to(const uint8_t *peer_mac, const uint8_t *data, size_t len);

    // Message structure placed on the API receive queue
    typedef struct
    {
        uint8_t src_mac[6];
        uint8_t data[256];
        int len;
    } espnow_msg_t;

    // Initialize receive queue with given length (defaults created in api_init if not called)
    esp_err_t espnow_api_recv_queue_init(size_t queue_len);

    // Pop a message from the API receive queue. Blocks up to ticks_to_wait (use portMAX_DELAY).
    // Returns ESP_OK when a message was retrieved, or ESP_ERR_TIMEOUT if timed out, or other esp_err_t on error.
    esp_err_t espnow_api_recv(espnow_msg_t *out_msg, TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif

#endif