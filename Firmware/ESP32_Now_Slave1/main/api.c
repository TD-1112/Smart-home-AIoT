// API implementation to keep main.c slim

#include <string.h>
#include "esp_log.h"
#include "api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char *API_TAG = "API";

// Store peer MAC configured at init
static uint8_t s_peer_mac[6] = {0};
static bool s_peer_set = false;

// Receive queue used to pass received messages out of the espnow callback
static QueueHandle_t s_recv_queue = NULL;

// Default queue length
#define API_RECV_QUEUE_LEN 10

// Internal callbacks for logging
static void api_on_data_recv(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    char msg[256] = {0};
    int cpy = len < (int)sizeof(msg) - 1 ? len : (int)sizeof(msg) - 1;
    if (cpy > 0 && data)
    {
        memcpy(msg, data, cpy);
        msg[cpy] = '\0';
    }
    // ESP_LOGI(API_TAG, "Recv from [%02X:%02X:%02X:%02X:%02X:%02X] => '%s'",
    //          mac_addr[0], mac_addr[1], mac_addr[2],
    //          mac_addr[3], mac_addr[4], mac_addr[5], msg);

    // Push copy into queue for processing in main
    if (s_recv_queue)
    {
        espnow_msg_t out = {0};
        memcpy(out.src_mac, mac_addr, 6);
        int copylen = len < (int)sizeof(out.data) ? len : (int)sizeof(out.data);
        if (copylen > 0 && data)
            memcpy(out.data, data, copylen);
        out.len = copylen;
        BaseType_t ok = xQueueSendFromISR(s_recv_queue, &out, NULL);
        if (ok != pdTRUE)
        {
            ESP_LOGW(API_TAG, "Receive queue full - dropping message");
        }
    }
}

static void api_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    ESP_LOGI(API_TAG, "Sent to [%02X:%02X:%02X:%02X:%02X:%02X] => %s",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5],
             (status == ESP_NOW_SEND_SUCCESS) ? "SUCCESS" : "FAIL");
}

void espnow_api_init(const uint8_t slave_mac[6])
{
    if (slave_mac)
    {
        memcpy(s_peer_mac, slave_mac, 6);
        s_peer_set = true;
    }
    else
    {
        s_peer_set = false;
    }

    espnow_config_t cfg = {
        .encrypt = false,
        .channel = 0,
    };
    my_espnow_init(&cfg);

    my_espnow_register_recv_cb(api_on_data_recv);
    my_espnow_register_send_cb(api_on_data_sent);

    if (s_peer_set)
    {
        my_espnow_add_peer(s_peer_mac, false);
    }

    if (!s_recv_queue)
    {
        s_recv_queue = xQueueCreate(API_RECV_QUEUE_LEN, sizeof(espnow_msg_t));
        if (!s_recv_queue)
        {
            ESP_LOGW(API_TAG, "Failed to create API recv queue");
        }
    }
}

esp_err_t espnow_api_send(const uint8_t *data, size_t len)
{
    if (!s_peer_set)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (!data || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return my_espnow_send(s_peer_mac, (const uint8_t *)data, len);
}

esp_err_t espnow_api_send_str(const char *msg)
{
    if (!msg)
        return ESP_ERR_INVALID_ARG;
    return espnow_api_send((const uint8_t *)msg, strlen(msg));
}

esp_err_t espnow_api_send_to(const uint8_t *peer_mac, const uint8_t *data, size_t len)
{
    if (!peer_mac)
        return ESP_ERR_INVALID_ARG;
    if (!data || len == 0)
        return ESP_ERR_INVALID_ARG;
    return my_espnow_send(peer_mac, data, len);
}

esp_err_t espnow_api_recv_queue_init(size_t queue_len)
{
    if (s_recv_queue)
        return ESP_OK; // already created
    size_t ql = queue_len ? queue_len : API_RECV_QUEUE_LEN;
    s_recv_queue = xQueueCreate(ql, sizeof(espnow_msg_t));
    return s_recv_queue ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t espnow_api_recv(espnow_msg_t *out_msg, TickType_t ticks_to_wait)
{
    if (!out_msg)
        return ESP_ERR_INVALID_ARG;
    if (!s_recv_queue)
        return ESP_ERR_INVALID_STATE;
    BaseType_t ok = xQueueReceive(s_recv_queue, out_msg, ticks_to_wait);
    return (ok == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}
