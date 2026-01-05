/**
 * @file speech_detect.c
 * @brief Speech detection module implementation
 */

#include "speech_detect.h"
#include "audio_feed.h"
#include "plug_control.h"
#include "app_config.h"

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_process_sdkconfig.h"
#include "model_path.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>
#include <assert.h>

static const char *TAG_WN = "[WN]";
static const char *TAG_MN = "[MN]";

/* ================== GLOBAL VARIABLES ================== */
srmodel_list_t *g_models = NULL;
volatile int g_detect_flag = 0;

/**
 * @brief Compare two strings for equality
 * @details Returns 1 if equal, 0 otherwise
 * @param str1
 * @param str2
 * @return int Returns 1 if strings are equal, 0 otherwise
 */
static int string_cmp(const char *str1, const char *str2)
{
    return (strcmp(str1, str2) == 0) ? 1 : 0;
}

/**
 * @brief Configure LED GPIO
 * @details Sets up the GPIO pin for LED output use for status indication of speech detection
 */
void led_config(void)
{
    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_LED, 0);
}

/**
 * @brief Initialize speech models
 * @return esp_err_t
 */
esp_err_t speech_models_init(void)
{
    g_models = esp_srmodel_init("model");
    if (g_models == NULL)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Initialize AFE for speech recognition
 * @details Configures and creates AFE instance for speech recognition with specified settings
 * @return esp_err_t
 */
esp_err_t speech_afe_init(void)
{
    g_afe = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;

    afe_config_t cfg = AFE_CONFIG_DEFAULT();
    // Set wakenet model (first wakenet)
    cfg.wakenet_model_name = esp_srmodel_filter(g_models, ESP_WN_PREFIX, NULL);

    cfg.aec_init = false; // Disable AEC
    cfg.se_init = false;  // Disable SE

    cfg.pcm_config.total_ch_num = 1; // mono input
    cfg.pcm_config.mic_num = 1;      // 1 mic channel
    cfg.pcm_config.ref_num = 0;      // 0 reference channel

    g_afe_data = g_afe->create_from_config(&cfg);
    if (g_afe_data == NULL)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Process recognized command
 * @details Maps recognized command strings to plug control actions
 * @param cmd_string
 */
static void process_command(const char *cmd_string)
{
    // ============ TURN ON PLUG 1 ============
    if (string_cmp(cmd_string, "TkN nN PLcG WcN") == 1 ||
        string_cmp(cmd_string, "Mb b MeT") == 1)
    {
        plug_set_flag(PLUG_1, STATE_ON);
        ESP_LOGI(TAG_MN, "PLUG 1 ON");
        plug_send_command(PLUG_1, STATE_ON);
    }
    // ============ TURN ON PLUG 2 ============
    else if (string_cmp(cmd_string, "TkN nN PLcG To") == 1 ||
             string_cmp(cmd_string, "Mb b hi") == 1)
    {
        plug_set_flag(PLUG_2, STATE_ON);
        ESP_LOGI(TAG_MN, "PLUG 2 ON");
        plug_send_command(PLUG_2, STATE_ON);
    }
    // ============ TURN ON PLUG 3 ============
    else if (string_cmp(cmd_string, "TkN nN PLcG vRm") == 1 ||
             string_cmp(cmd_string, "Mb b Bn") == 1)
    {
        plug_set_flag(PLUG_3, STATE_ON);
        ESP_LOGI(TAG_MN, "PLUG 3 ON");
        plug_send_command(PLUG_3, STATE_ON);
    }
    // ============ TURN OFF PLUG 1 ============
    else if (string_cmp(cmd_string, "TkN eF PLcG WcN") == 1 ||
             string_cmp(cmd_string, "TaT b MeT") == 1)
    {
        plug_set_flag(PLUG_1, STATE_OFF);
        ESP_LOGI(TAG_MN, "PLUG 1 OFF");
        plug_send_command(PLUG_1, STATE_OFF);
    }
    // ============ TURN OFF PLUG 2 ============
    else if (string_cmp(cmd_string, "TkN eF PLcG To") == 1 ||
             string_cmp(cmd_string, "TaT b hi") == 1)
    {
        plug_set_flag(PLUG_2, STATE_OFF);
        ESP_LOGI(TAG_MN, "PLUG 2 OFF");
        plug_send_command(PLUG_2, STATE_OFF);
    }
    // ============ TURN OFF PLUG 3 ============
    else if (string_cmp(cmd_string, "TkN eF PLcG vRm") == 1 ||
             string_cmp(cmd_string, "TaT b Bn") == 1)
    {
        plug_set_flag(PLUG_3, STATE_OFF);
        ESP_LOGI(TAG_MN, "PLUG 3 OFF");
        plug_send_command(PLUG_3, STATE_OFF);
    }
    // ============ TURN ON ALL PLUGS ============
    else if (string_cmp(cmd_string, "TkN nN eL PLcGZ") == 1 ||
             string_cmp(cmd_string, "Mb TaT Kc") == 1)
    {
        plug_set_all_flags(STATE_ON);
        ESP_LOGI(TAG_MN, "ALL PLUGS ON");
        plug_send_command(PLUG_1, STATE_ON);
        plug_send_command(PLUG_2, STATE_ON);
        plug_send_command(PLUG_3, STATE_ON);
    }
    // ============ TURN OFF ALL PLUGS ============
    else if (string_cmp(cmd_string, "TkN eF eL PLcGZ") == 1 ||
             string_cmp(cmd_string, "TaT TaT Kc") == 1)
    {
        plug_set_all_flags(STATE_OFF);
        ESP_LOGI(TAG_MN, "ALL PLUGS OFF");
        plug_send_command(PLUG_1, STATE_OFF);
        plug_send_command(PLUG_2, STATE_OFF);
        plug_send_command(PLUG_3, STATE_OFF);
    }
}
/**
 * @brief Speech detection task
 * @details Main loop for wake word detection and command recognition
 * @param arg
 */
void detect_task(void *arg)
{
    led_config();

    int fetch_chunk = g_afe->get_fetch_chunksize(g_afe_data);

    // Initialize MultiNet
    char *mn_name = esp_srmodel_filter(g_models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    ESP_LOGI(TAG_MN, "MultiNet: %s", mn_name);
    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_data = multinet->create(mn_name, 6000);

    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    assert(mu_chunksize == fetch_chunk);

    // Add speech commands from sdkconfig
    esp_mn_commands_update_from_sdkconfig(multinet, model_data);
    multinet->print_active_speech_commands(model_data);

    ESP_LOGI(TAG_WN, "---- AWAIT WAKE WORD ----");

    while (g_task_flag)
    {
        afe_fetch_result_t *res = g_afe->fetch(g_afe_data);
        if (!res || res->ret_value == ESP_FAIL)
        {
            ESP_LOGE(TAG_WN, "fetch error!");
            break;
        }

        if (res->wakeup_state == WAKENET_DETECTED)
        {
            ESP_LOGI(TAG_WN, "WAKE WORD DETECTED");
            multinet->clean(model_data);
        }

        if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED)
        {
            ESP_LOGI(TAG_WN, "CHANNEL VERIFIED, channel index: %d", res->trigger_channel_id);
            g_detect_flag = 1;
            gpio_set_level(GPIO_LED, 1); // LED ON when listening for command
        }

        if (g_detect_flag == 1)
        {
            esp_mn_state_t mn_state = multinet->detect(model_data, res->data);

            if (mn_state == ESP_MN_STATE_DETECTING)
            {
                continue;
            }

            if (mn_state == ESP_MN_STATE_DETECTED)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                for (int i = 0; i < mn_result->num; i++)
                {
                    ESP_LOGI(TAG_MN, "TOP %d, command_id: %d, phrase_id: %d, string: %s, prob: %f",
                             i + 1, mn_result->command_id[i], mn_result->phrase_id[i],
                             mn_result->string, mn_result->prob[i]);

                    process_command(mn_result->string);
                }
                ESP_LOGI(TAG_MN, "-----------listening-----------");
            }

            if (mn_state == ESP_MN_STATE_TIMEOUT)
            {
                esp_mn_results_t *mn_result = multinet->get_results(model_data);
                ESP_LOGI(TAG_MN, "TIMEOUT, string: %s", mn_result->string);
                g_afe->enable_wakenet(g_afe_data);
                g_detect_flag = 0;
                gpio_set_level(GPIO_LED, 0); // LED OFF when waiting for wake word
                ESP_LOGI(TAG_WN, "---- AWAIT WAKE WORD ----");
                continue;
            }
        }
    }

    if (model_data)
    {
        multinet->destroy(model_data);
        model_data = NULL;
    }
    ESP_LOGI(TAG_MN, "Detect task exit");
    vTaskDelete(NULL);
}
