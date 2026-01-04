#include "my_espnow.h"
#include <string.h>
#include "nvs_flash.h"

static const char *TAG = "my_espnow";

// Set to 1 to enable verbose RX preview/hexdumps and send status logs.
#ifndef MY_ESPNOW_VERBOSE
#define MY_ESPNOW_VERBOSE 0
#endif

static espnow_recv_callback_t user_recv_cb = NULL;
static espnow_send_callback_t user_send_cb = NULL;

static void internal_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    (void)TAG;

#if MY_ESPNOW_VERBOSE
    if (info && info->src_addr)
    {
        ESP_LOGI(TAG, "RX from [%02X:%02X:%02X:%02X:%02X:%02X] len=%d",
                 info->src_addr[0], info->src_addr[1], info->src_addr[2],
                 info->src_addr[3], info->src_addr[4], info->src_addr[5],
                 len);

        if (data && len > 0)
        {
            // Print a bounded string preview (useful for JSON)
            char preview[201];
            size_t copy_len = (size_t)len;
            if (copy_len > sizeof(preview) - 1)
            {
                copy_len = sizeof(preview) - 1;
            }
            memcpy(preview, data, copy_len);
            preview[copy_len] = '\0';
            ESP_LOGI(TAG, "RX payload(str)=%s", preview);

            size_t dump_len = (size_t)len;
            if (dump_len > 64)
            {
                dump_len = 64;
            }
            ESP_LOG_BUFFER_HEXDUMP(TAG, data, dump_len, ESP_LOG_INFO);
        }
    }
#endif

    if (user_recv_cb && info && info->src_addr)
        user_recv_cb(info->src_addr, data, len);
}

static void internal_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    (void)status;

#if MY_ESPNOW_VERBOSE
    if (mac_addr)
    {
        ESP_LOGI(TAG, "Send to [%02X:%02X:%02X:%02X:%02X:%02X] -> %s",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5],
                 status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
    }
#endif
    if (user_send_cb)
    {
        user_send_cb(mac_addr, status);
    }
}

esp_err_t my_espnow_init(const espnow_config_t *config)
{
    esp_err_t ret;

    ret = esp_now_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_now_register_recv_cb(internal_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(internal_send_cb));

    ESP_LOGI(TAG, "ESP-NOW init success (use STA channel)");
    return ESP_OK;
}

esp_err_t my_espnow_add_peer(const uint8_t *peer_mac, uint8_t channel, bool encrypt)
{
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, peer_mac, 6);

    peer.channel = channel;
    peer.encrypt = encrypt;
    peer.ifidx = ESP_IF_WIFI_STA;

    return esp_now_add_peer(&peer);
}

esp_err_t my_espnow_del_peer(const uint8_t *peer_mac)
{
    return esp_now_del_peer(peer_mac);
}

esp_err_t my_espnow_send(const uint8_t *peer_mac, const uint8_t *data, size_t len)
{
    return esp_now_send(peer_mac, data, len);
}

void my_espnow_register_recv_cb(espnow_recv_callback_t cb)
{
    user_recv_cb = cb;
}

void my_espnow_register_send_cb(espnow_send_callback_t cb)
{
    user_send_cb = cb;
}
