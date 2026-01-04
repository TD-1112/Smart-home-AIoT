#include "api.h"
#include <string.h>

static const char *TAG = "ESPNOW_API";

static QueueHandle_t espnow_recv_queue = NULL;

/**
 * @brief Callback for ESP-NOW send events.
 * @details Logs the result of the send operation.
 * @param mac_addr
 * @param status
 */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        ESP_LOGI(TAG, "ESP-NOW send success");
    }
    else
    {
        ESP_LOGW(TAG, "ESP-NOW send fail");
    }
}

/**
 * @brief Callback for ESP-NOW send events.
 * @details Logs the result of the send operation.
 * @param mac_addr
 * @param data
 * @param len
 */
static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    if (!mac_addr || !data || len <= 0)
        return;

    espnow_msg_t msg = {0};
    memcpy(msg.src_mac, mac_addr, 6);
    msg.len = len > sizeof(msg.data) ? sizeof(msg.data) : len;
    memcpy(msg.data, data, msg.len);

    if (espnow_recv_queue)
    {
        if (xQueueSend(espnow_recv_queue, &msg, 0) != pdTRUE)
        {
            ESP_LOGW(TAG, "Recv queue full, dropping packet");
        }
    }
}

/**
 * @brief function to initialize the ESP-NOW API.
 */
void espnow_api_init(void)
{
    espnow_config_t cfg = {
        .encrypt = false,
        .channel = 1,
    };

    ESP_ERROR_CHECK(my_espnow_init(&cfg));

    my_espnow_register_recv_cb(espnow_recv_cb);
    my_espnow_register_send_cb(espnow_send_cb);

    espnow_api_recv_queue_init(10);
}

/**
 * @brief Initialize the ESP-NOW receive queue.
 * @details Creates a FreeRTOS queue to hold incoming ESP-NOW messages.
 * @param queue_len
 */
void espnow_api_recv_queue_init(uint16_t queue_len)
{
    if (espnow_recv_queue)
    {
        vQueueDelete(espnow_recv_queue);
    }

    espnow_recv_queue = xQueueCreate(queue_len, sizeof(espnow_msg_t));
    if (espnow_recv_queue)
        ;
    else
    {
        ESP_LOGE(TAG, "Failed to create ESP-NOW receive queue");
    }
}

/**
 * @brief Add a peer to ESP-NOW.
 * @details Adds a peer device with the specified MAC address, channel, and encryption setting.
 * @param peer_mac
 * @param channel
 * @param encrypt
 * @return esp_err_t
 */
esp_err_t espnow_api_add_peer(const uint8_t peer_mac[6], uint8_t channel, bool encrypt)
{
    if (!peer_mac)
        return ESP_ERR_INVALID_ARG;

    return my_espnow_add_peer(peer_mac, channel, encrypt);
}

/**
 * @brief Delete a peer from ESP-NOW.
 * @details Removes a peer device with the specified MAC address.
 * @param peer_mac The MAC address of the peer to delete.
 * @return esp_err_t
 */
esp_err_t espnow_api_del_peer(const uint8_t peer_mac[6])
{
    if (!peer_mac)
        return ESP_ERR_INVALID_ARG;

    return my_espnow_del_peer(peer_mac);
}

esp_err_t espnow_api_send_to(const uint8_t peer_mac[6], const uint8_t *data, size_t len)
{
    if (!peer_mac || !data || len == 0)
        return ESP_ERR_INVALID_ARG;

    return my_espnow_send(peer_mac, data, len);
}

esp_err_t espnow_api_recv(espnow_msg_t *msg, TickType_t timeout)
{
    if (!espnow_recv_queue || !msg)
        return ESP_FAIL;

    if (xQueueReceive(espnow_recv_queue, msg, timeout) == pdTRUE)
        return ESP_OK;

    return ESP_ERR_TIMEOUT;
}
