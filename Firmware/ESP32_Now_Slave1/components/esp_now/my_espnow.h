#ifndef __MY_ESPNOW_H__
#define __MY_ESPNOW_H__

#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*espnow_recv_callback_t)(const uint8_t *mac_addr, const uint8_t *data, int len);
    typedef void (*espnow_send_callback_t)(const uint8_t *mac_addr, esp_now_send_status_t status);

    typedef struct
    {
        bool encrypt;
        uint8_t channel;
    } espnow_config_t;

    esp_err_t my_espnow_init(const espnow_config_t *config);
    esp_err_t my_espnow_add_peer(const uint8_t *peer_mac, bool encrypt);
    esp_err_t my_espnow_send(const uint8_t *peer_mac, const uint8_t *data, size_t len);
    void my_espnow_register_recv_cb(espnow_recv_callback_t cb);
    void my_espnow_register_send_cb(espnow_send_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif // __MY_ESPNOW_H__
