#include "esp_log.h"
#include "api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Json_message.h"
#include "my_wifi.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "uart_protocol.h"
#include "lib_uart.h"
#include "define.h"
#include "helper_function.h"

/**
 * @brief convert mac to string
 * @param mac
 * @param str
 */
void mac_to_string(const uint8_t *mac, char *str)
{
    sprintf(str, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief check if slave is already discovered
 * @details This function checks if a slave with the given MAC address is already in the discovered_slaves list.
 * @param mac
 * @return true
 * @return false
 */
bool is_slave_discovered(const uint8_t *mac)
{
    for (int i = 0; i < slave_count; i++)
    {
        if (memcmp(discovered_slaves[i].mac, mac, 6) == 0)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief remove slave when send fail
 * @details This function removes a slave from the discovered_slaves list and deletes it from the ESP-NOW peer list when a send failure occurs.
 * @param mac Pointer to the MAC address of the slave to be removed.
 */
void remove_slave(const uint8_t *mac)
{
    for (int i = 0; i < slave_count; i++)
    {
        if (memcmp(discovered_slaves[i].mac, mac, 6) == 0)
        {
            ESP_LOGW(Master_Tag, "Removing slave '%s' due to send failure.", discovered_slaves[i].name);
            espnow_api_del_peer(mac);

            // Shift remaining slaves down
            for (int j = i; j < slave_count - 1; j++)
            {
                discovered_slaves[j] = discovered_slaves[j + 1];
            }
            slave_count--;
            return;
        }
    }
}

/**
 * @brief add new slave to discovered list and espnow peer list
 * @details This function adds a new slave to the discovered_slaves list and registers it as an ESP-NOW peer.
 * @param mac Pointer to the MAC address of the new slave.
 * @param name
 */
void add_new_slave(const uint8_t *mac, const char *name)
{
    if (slave_count < MAX_SLAVES && !is_slave_discovered(mac))
    {
        esp_err_t ret = espnow_api_add_peer(mac, ESP_NOW_WIFI_CHANNEL, false);
        if (ret == ESP_OK || ret == ESP_ERR_ESPNOW_EXIST)
        {
            memcpy(discovered_slaves[slave_count].mac, mac, 6);
            strncpy(discovered_slaves[slave_count].name, name, SLAVE_NAME_LEN - 1);
            discovered_slaves[slave_count].name[SLAVE_NAME_LEN - 1] = '\0'; // Ensure null-termination
            slave_count++;
            ESP_LOGI(Master_Tag, "Added new slave '%s' with MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                     name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

            // --- Send confirmation message ---
            json_master_msg_t confirm_msg = {
                .type = JSON_MSG_TYPE_CONTROL,
                .cmd = JSON_CMD_REGISTER_SUCCESS,
                .has_cmd = true};
            mac_to_string(MASTER_MAC, confirm_msg.id);
            mac_to_string(mac, confirm_msg.dst);

            char *json_msg = json_encode_master_msg(&confirm_msg);
            if (json_msg)
            {
                espnow_api_send_to(mac, (const uint8_t *)json_msg, strlen(json_msg));
                ESP_LOGI(Master_Tag, "Sent registration confirmation to %s", name);
                free(json_msg);
            }
        }
        else
        {
            ESP_LOGE(Master_Tag, "Failed to add slave peer: %02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    }
}

/**
 * @brief funtion send callback
 *
 * @param mac_addr
 * @param status
 */
void master_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status != ESP_NOW_SEND_SUCCESS)
    {
        ESP_LOGE(Master_Tag, "Send to %02X:%02X:%02X:%02X:%02X:%02X failed",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        // If not broadcast MAC, remove the slave
        if (memcmp(mac_addr, BROADCAST_MAC, 6) != 0)
        {
            remove_slave(mac_addr);
        }
    }
}
