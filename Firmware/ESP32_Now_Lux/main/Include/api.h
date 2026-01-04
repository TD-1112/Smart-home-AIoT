#ifndef __API_ESPNOW_H__
#define __API_ESPNOW_H__

#include "my_espnow.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        uint8_t src_mac[6];
        uint8_t data[250];
        size_t len;
    } espnow_msg_t;

    /**
     * @brief Khởi tạo lớp API của ESP-NOW
     */
    void espnow_api_init(void);

    /**
     * @brief Thêm một peer vào danh sách
     */
    esp_err_t espnow_api_add_peer(const uint8_t peer_mac[6], uint8_t channel, bool encrypt);

    /**
     * @brief Xóa một peer khỏi danh sách
     */
    esp_err_t espnow_api_del_peer(const uint8_t peer_mac[6]);

    /**
     * @brief Gửi dữ liệu đến một peer cụ thể
     */
    esp_err_t espnow_api_send_to(const uint8_t peer_mac[6], const uint8_t *data, size_t len);

    /**
     * @brief Nhận dữ liệu (blocking hoặc timeout)
     */
    esp_err_t espnow_api_recv(espnow_msg_t *msg, TickType_t timeout);

    /**
     * @brief Khởi tạo hàng đợi nhận (nếu bạn muốn đổi kích thước queue)
     */
    void espnow_api_recv_queue_init(uint16_t queue_len);

#ifdef __cplusplus
}
#endif

#endif // __API_ESPNOW_H__
