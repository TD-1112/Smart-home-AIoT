#include "api.h"
#include <string.h>

static const char *TAG = "ESPNOW_API";

// Set to 1 to enable verbose TX/RX payload logs + hexdumps.
#ifndef ESPNOW_API_VERBOSE
#define ESPNOW_API_VERBOSE 0
#endif

static QueueHandle_t espnow_recv_queue = NULL;

/**
 * @brief Callback for ESP-NOW send events.
 * @details Logs the result of the send operation.
 * @param mac_addr
 * @param data
 * @param len
 */
static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len);
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);

#if ESPNOW_API_VERBOSE
static void log_mac_and_payload(const char *tag, const char *prefix, const uint8_t mac_addr[6], const uint8_t *data, size_t len)
{
    if (!prefix)
    {
        prefix = "";
    }

    if (!mac_addr)
    {
        ESP_LOGI(tag, "%s MAC=<null> len=%u", prefix, (unsigned)len);
    }
    else
    {
        ESP_LOGI(tag, "%s [%02X:%02X:%02X:%02X:%02X:%02X] len=%u", prefix,
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5],
                 (unsigned)len);
    }

    if (!data || len == 0)
    {
        ESP_LOGI(tag, "%s <empty>", prefix);
        return;
    }

    // Print as string (bounded) + short hexdump (bounded)
    char strbuf[201];
    size_t copy_len = len;
    if (copy_len > sizeof(strbuf) - 1)
    {
        copy_len = sizeof(strbuf) - 1;
    }
    memcpy(strbuf, data, copy_len);
    strbuf[copy_len] = '\0';

    ESP_LOGI(tag, "%s payload(str)=%s", prefix, strbuf);

    size_t dump_len = len;
    if (dump_len > 64)
    {
        dump_len = 64;
    }
    ESP_LOG_BUFFER_HEXDUMP(tag, data, dump_len, ESP_LOG_INFO);
}
#endif

static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    if (!mac_addr || !data || len <= 0)
        return;

#if ESPNOW_API_VERBOSE
    log_mac_and_payload(TAG, "RX", mac_addr, data, (size_t)len);
#endif

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

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    (void)mac_addr;
    (void)status;
    // Có thể log hoặc xử lý trạng thái gửi ở đây nếu cần
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

#if ESPNOW_API_VERBOSE
    log_mac_and_payload(TAG, "TX", peer_mac, data, len);
#endif

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

void espnow_api_register_send_cb(espnow_send_callback_t cb)
{
    my_espnow_register_send_cb(cb);
}
