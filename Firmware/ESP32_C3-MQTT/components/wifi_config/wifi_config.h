#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include "esp_err.h"

// Define a callback type for when WiFi connects successfully
typedef void (*wifi_connected_cb_t)(void);

// Enum to signal the system state to other tasks (like the LED task)
typedef enum {
    WIFI_STATE_UNINITIALIZED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE,
    WIFI_STATE_MQTT_CONNECTED, // Added for LED logic
    WIFI_STATE_ERROR
} wifi_manager_state_t;

/**
 * @brief Starts the WiFi manager.
 *
 * This function attempts to connect to any of the saved WiFi networks.
 * If no networks are saved, or if it fails to connect to all saved networks,
 * it will start an Access Point (AP) mode to allow for configuration.
 *
 * @param callback Function to be called when WiFi connection is established.
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t wifi_config_start(wifi_connected_cb_t callback);

/**
 * @brief Gets the current state of the WiFi manager.
 *
 * This is used to signal the status to other parts of the application,
 * for example, to control an LED.
 *
 * @return wifi_manager_state_t The current state.
 */
wifi_manager_state_t wifi_config_get_state(void);

/**
 * @brief Sets the current state of the WiFi manager.
 *
 * This function is thread-safe.
 *
 * @param state The new state to set.
 */
void wifi_config_set_state(wifi_manager_state_t state);

/**
 * @brief Deletes all saved WiFi credentials from NVS.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise.
 */
esp_err_t wifi_config_reset(void);

#endif // WIFI_CONFIG_H
