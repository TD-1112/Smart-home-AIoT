/**
 * @file audio_feed.h
 * @brief Audio feed module - I2S and AFE feed task
 */

#ifndef AUDIO_FEED_H
#define AUDIO_FEED_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_afe_sr_iface.h"
#include "driver/i2s_std.h"

/* ================== GLOBAL VARIABLES ================== */
extern esp_afe_sr_iface_t *g_afe;
extern esp_afe_sr_data_t *g_afe_data;
extern i2s_chan_handle_t g_rx_chan;
extern volatile int g_task_flag;

/* ================== FUNCTION PROTOTYPES ================== */

/**
 * @brief Initialize I2S for audio input
 * @return ESP_OK on success
 */
esp_err_t audio_i2s_init(void);

/**
 * @brief Feed task - Read audio from I2S and feed to AFE
 * @param arg Task argument (unused)
 */
void feed_task(void *arg);

#endif /* AUDIO_FEED_H */
