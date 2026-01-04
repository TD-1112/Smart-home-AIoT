#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "driver/gpio.h"

#include "api.h"
#include "my_wifi.h"
#include "Json_message.h"
#include "esp32-dht11.h"
#include "esp_now.h"
#include "cJSON.h"

// --- Configuration ---
#define SLAVE_NAME "DHT11_Sensor_1"
#define LED_PIN (GPIO_NUM_2)
#define DHT_PIN (GPIO_NUM_3)
#define MASTER_CONNECTION_TIMEOUT_MS 5000 // 5 seconds
#define ESP_NOW_WIFI_CHANNEL 1            // Define a fixed channel for ESP-NOW

// --- Global Variables ---
static const char *TAG = "SLAVE";
static const uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t s_slave_mac[6];
static uint8_t s_master_mac[6];
static bool s_is_master_paired = false;
static volatile TickType_t s_last_msg_recv_time;

// --- Forward Declarations ---
static void handle_data_request(const espnow_msg_t *msg);
static void send_discovery_response(const uint8_t *mac_addr);
static void espnow_process_task(void *pvParameter);
static void connection_check_task(void *pvParameter);
static void slave_discovery_broadcast_task(void *pvParameter);

// --- Helper Functions ---

/**
 * @brief Converts a MAC address to a string representation.
 *
 * @param mac Pointer to the MAC address array.
 * @param str Buffer to store the resulting string. Must be at least 18 bytes.
 */
static void mac_to_string(const uint8_t *mac, char *str)
{
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief Creates and sends a standard discovery response to the given MAC address.
 */
static void send_discovery_response(const uint8_t *mac_addr)
{
    json_slave_disc_resp_t resp;
    resp.type = JSON_MSG_TYPE_DISCOVERY_RESPONSE;
    mac_to_string(s_slave_mac, resp.id);
    strncpy(resp.name, SLAVE_NAME, sizeof(resp.name) - 1);
    resp.name[sizeof(resp.name) - 1] = '\0'; // Ensure null termination

    char *json_str = json_encode_slave_discovery_response(&resp);
    if (json_str)
    {
        ESP_LOGI(TAG, "--> SENDING DISCOVERY RESPONSE to %02X:%02X:%02X:%02X:%02X:%02X",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        ESP_LOGD(TAG, "Response JSON: %s", json_str);
        espnow_api_send_to(mac_addr, (const uint8_t *)json_str, strlen(json_str));
        free(json_str);
    }
}

/**
 * @brief Handles a data request from the master by reading the sensor and sending a response.
 */
static void handle_data_request(const espnow_msg_t *msg)
{
    // Basic check to ensure the request is from the paired master
    if (s_is_master_paired && memcmp(msg->src_mac, s_master_mac, 6) != 0)
    {
        ESP_LOGW(TAG, "Received data request from an unknown MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 msg->src_mac[0], msg->src_mac[1], msg->src_mac[2], msg->src_mac[3], msg->src_mac[4], msg->src_mac[5]);
        return;
    }

    dht11_t dht11 = {
        .dht11_pin = DHT_PIN,
        .temperature = 0,
        .humidity = 0};
    if (dht11_read(&dht11, 100) == 0) // 0 = success, 100 = timeout (adjust as needed)
    {
        ESP_LOGI(TAG, "Master requested data. Temp = %.1fC, Humi = %.1f%%", dht11.temperature, dht11.humidity);

        json_slave_data_t resp;
        resp.type = JSON_MSG_TYPE_RESPONSE_DATA;
        mac_to_string(s_slave_mac, resp.id);
        mac_to_string(msg->src_mac, resp.dst);
        resp.data.temp = (int)dht11.temperature;
        resp.data.humi = (int)dht11.humidity;

        char *json_str = json_encode_slave_data(&resp);
        if (json_str)
        {
            ESP_LOGI(TAG, "--> SENDING DATA RESPONSE");
            ESP_LOGD(TAG, "Response JSON: %s", json_str);
            espnow_api_send_to(msg->src_mac, (const uint8_t *)json_str, strlen(json_str));
            free(json_str);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read from DHT11 sensor.");
    }
}

/**
 * @brief Task to process incoming ESP-NOW messages.
 * @details Listens for messages from the master, handles discovery and data requests.
 * @param pvParameter
 */
static void espnow_process_task(void *pvParameter)
{
    espnow_msg_t msg;
    ESP_LOGI(TAG, "ESP-NOW processing task started. Waiting for discovery from master...");
    while (1)
    {
        if (espnow_api_recv(&msg, portMAX_DELAY) == ESP_OK)
        {
            s_last_msg_recv_time = xTaskGetTickCount();
            msg.data[msg.len] = '\0';

            ESP_LOGI(TAG, "<-- RECEIVED RAW MESSAGE from %02X:%02X:%02X:%02X:%02X:%02X: %s",
                     msg.src_mac[0], msg.src_mac[1], msg.src_mac[2], msg.src_mac[3], msg.src_mac[4], msg.src_mac[5],
                     (char *)msg.data);

            json_master_msg_t *master_msg = json_decode_master_msg((const char *)msg.data);
            if (!master_msg)
            {
                ESP_LOGW(TAG, "Failed to decode JSON message.");
                continue;
            }

            // --- Main Logic ---
            switch (master_msg->type)
            {
            case JSON_MSG_TYPE_DISCOVERY:
                if (!s_is_master_paired)
                {
                    ESP_LOGI(TAG, "Received discovery broadcast. Responding...");
                    // Add master as a temporary peer to send response
                    espnow_api_add_peer(msg.src_mac, 0, false);
                    send_discovery_response(msg.src_mac);
                }
                break;

            case JSON_MSG_TYPE_ASK_DATA:
                if (!s_is_master_paired)
                {
                    ESP_LOGI(TAG, "First data request from master. Pairing now!");
                    s_is_master_paired = true;
                    memcpy(s_master_mac, msg.src_mac, 6);
                    gpio_set_level(LED_PIN, 1);

                    // Ensure peer is properly added
                    espnow_api_add_peer(s_master_mac, 0, false);

                    // We are paired, stop listening to broadcasts.
                    ESP_LOGI(TAG, "Stopping listening to broadcast messages.");
                    espnow_api_del_peer(s_broadcast_mac);
                }
                handle_data_request(&msg);
                break;

            case JSON_MSG_TYPE_CONTROL:
                if (!s_is_master_paired)
                {
                    s_is_master_paired = true;
                    memcpy(s_master_mac, msg.src_mac, 6);
                    gpio_set_level(LED_PIN, 1);
                    espnow_api_add_peer(s_master_mac, 0, false);
                }
                ESP_LOGI(TAG, "Received CONTROL message. Stopping listening to broadcast messages.");
                espnow_api_del_peer(s_broadcast_mac);
                // Có thể xử lý thêm nội dung CMD nếu cần
                break;

            default:
                ESP_LOGW(TAG, "Received unknown message type: %d", master_msg->type);
                break;
            }

            free(master_msg);
        }
        else
        {
            ESP_LOGW(TAG, "espnow_api_recv failed");
        }
    }
}

/**
 * @brief Task to monitor connection with master and handle timeouts.
 * @details If no messages are received from the master within the timeout period,
 *          the slave will un-pair and return to discovery mode.
 * @param pvParameter
 */
static void connection_check_task(void *pvParameter)
{
    int led_state = 0;
    ESP_LOGI(TAG, "Connection check task started.");
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second

        if (s_is_master_paired)
        {
            // Solid LED when paired
            gpio_set_level(LED_PIN, 1);
            if ((xTaskGetTickCount() - s_last_msg_recv_time) > pdMS_TO_TICKS(MASTER_CONNECTION_TIMEOUT_MS))
            {
                ESP_LOGW(TAG, "Master connection lost. Returning to discovery mode.");

                // Un-pair
                s_is_master_paired = false;

                // Delete old master peer
                espnow_api_del_peer(s_master_mac);

                // Re-add broadcast MAC to listen for discovery messages again
                esp_err_t add_ret = espnow_api_add_peer(s_broadcast_mac, 0, false);
                if (add_ret != ESP_OK && add_ret != ESP_ERR_ESPNOW_EXIST)
                {
                    ESP_LOGE(TAG, "Failed to re-add broadcast peer, error: %d", add_ret);
                }
                else
                {
                    ESP_LOGI(TAG, "Now listening for discovery broadcasts again.");
                }
            }
        }
        else
        {
            // Blinking LED when not paired
            led_state = !led_state;
            gpio_set_level(LED_PIN, led_state);
        }
    }
}

/**
 * @brief function to broadcast discovery responses periodically when unpaired.
 *
 * @param pvParameter
 */
static void slave_discovery_broadcast_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Slave Discovery Broadcast task started.");
    while (1)
    {
        if (!s_is_master_paired)
        {
            json_slave_disc_resp_t resp;
            resp.type = JSON_MSG_TYPE_DISCOVERY_RESPONSE;
            mac_to_string(s_slave_mac, resp.id);
            strncpy(resp.name, SLAVE_NAME, sizeof(resp.name) - 1);
            resp.name[sizeof(resp.name) - 1] = '\0';

            char *json_str = json_encode_slave_discovery_response(&resp);
            if (json_str)
            {
                ESP_LOGI(TAG, "--> BROADCASTING DISCOVERY RESPONSE (unpaired)");
                espnow_api_send_to(s_broadcast_mac, (const uint8_t *)json_str, strlen(json_str));
                free(json_str);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to encode discovery response JSON.");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000)); // Broadcast every 5 seconds
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // No specific init for DHT11 sensor is required by some libraries
    // as the pin is specified in the read function.
    // If your library requires it, add it here e.g. dht_init(DHT_PIN);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED_PIN, 0);

    // Initialize Wi-Fi in ESP-NOW mode
    esp_err_t res = wifi_init_for_esp_now();
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize Wi-Fi for ESP-NOW, error: %d", res);
        return;
    }

    ESP_LOGI(TAG, "Setting Wi-Fi channel to %d", ESP_NOW_WIFI_CHANNEL);
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESP_NOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    // Get and log the slave MAC address
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, s_slave_mac));
    char mac_str[18];
    mac_to_string(s_slave_mac, mac_str);
    ESP_LOGI(TAG, "ESP32 DHT11 Slave initialized. MAC: %s", mac_str);

    espnow_api_init();

    // Add broadcast peer initially to listen for discovery messages
    ESP_LOGI(TAG, "Adding broadcast peer to listen for master.");
    esp_err_t add_ret = espnow_api_add_peer(s_broadcast_mac, 0, false);
    if (add_ret != ESP_OK && add_ret != ESP_ERR_ESPNOW_EXIST)
    {
        ESP_LOGE(TAG, "Failed to add broadcast peer, error: %d", add_ret);
    }

    s_last_msg_recv_time = xTaskGetTickCount();

    xTaskCreate(espnow_process_task, "espnow_proc_task", 4096, NULL, 4, NULL);
    xTaskCreate(connection_check_task, "conn_check_task", 2048, NULL, 3, NULL);
    xTaskCreate(slave_discovery_broadcast_task, "slave_disc_bcast_task", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Setup complete. app_main task will now be deleted.");

    vTaskDelete(NULL);
}