#include "my_espnow.h"
#include <string.h>
#include "nvs_flash.h"

static const char *TAG = "my_espnow";

static espnow_recv_callback_t user_recv_cb = NULL;
static espnow_send_callback_t user_send_cb = NULL;

// ==== INTERNAL CALLBACKS ====
static void internal_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (user_recv_cb && info && info->src_addr)
        user_recv_cb(info->src_addr, data, len);
}

static void internal_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (user_send_cb)
        user_send_cb(mac_addr, status);
}

// ==== PUBLIC API IMPLEMENTATION ====
esp_err_t my_espnow_init(const espnow_config_t *config)
{
    // Initialize NVS partition (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ret = esp_now_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_now_register_recv_cb(internal_recv_cb);
    esp_now_register_send_cb(internal_send_cb);

    ESP_LOGI(TAG, "ESP-NOW init success");
    return ESP_OK;
}

esp_err_t my_espnow_add_peer(const uint8_t *peer_mac, bool encrypt)
{
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, peer_mac, 6);
    peer.channel = 0;
    peer.encrypt = encrypt;
    esp_err_t res = esp_now_add_peer(&peer);

    if (res == ESP_OK)
        ESP_LOGI(TAG, "Added peer: %02X:%02X:%02X:%02X:%02X:%02X",
                 peer_mac[0], peer_mac[1], peer_mac[2],
                 peer_mac[3], peer_mac[4], peer_mac[5]);
    else
        ESP_LOGE(TAG, "Failed to add peer: %s", esp_err_to_name(res));
    return res;
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
