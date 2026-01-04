/**
 * @file speech_detect.h
 * @brief Speech detection module - WakeNet and MultiNet processing
 */

#ifndef SPEECH_DETECT_H
#define SPEECH_DETECT_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "model_path.h"
#include "esp_afe_sr_models.h"

/* ================== GLOBAL VARIABLES ================== */
extern srmodel_list_t *g_models;
extern volatile int g_detect_flag;

/* ================== FUNCTION PROTOTYPES ================== */

/**
 * @brief Initialize speech recognition models
 * @return ESP_OK on success
 */
esp_err_t speech_models_init(void);

/**
 * @brief Initialize AFE (Audio Front End)
 * @return ESP_OK on success
 */
esp_err_t speech_afe_init(void);

/**
 * @brief LED configuration for status indication
 */
void led_config(void);

/**
 * @brief Detect task - Process WakeNet and MultiNet
 * @param arg Task argument (unused)
 */
void detect_task(void *arg);

#endif /* SPEECH_DETECT_H */
