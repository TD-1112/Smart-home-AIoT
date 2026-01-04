#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "esp_system.h"

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"

/* ================== I2S PIN ================== */
#define I2S_WS GPIO_NUM_4
#define I2S_SCK GPIO_NUM_5
#define I2S_SD GPIO_NUM_6

static const char *TAG = "SR";

/* ================== GLOBAL ================== */
static esp_afe_sr_iface_t *afe = NULL;
static esp_afe_sr_data_t *afe_data = NULL;
static srmodel_list_t *models = NULL;
static i2s_chan_handle_t rx_chan;
static volatile int task_flag = 1;

/* ================== I2S INIT ================== */
static void i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_0, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = 32,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK,
            .ws = I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_SD,
            .invert_flags = {
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    ESP_LOGI(TAG, "I2S init OK");
}

/* ================== FEED TASK ================== */
static void feed_task(void *arg)
{
    int chunk = afe->get_feed_chunksize(afe_data);

    int32_t *i2s_buf = malloc(chunk * sizeof(int32_t));
    int16_t *afe_buf = malloc(chunk * sizeof(int16_t));
    assert(i2s_buf && afe_buf);

    ESP_LOGI(TAG, "AFE feed chunk = %d", chunk);

    while (task_flag)
    {
        int collected = 0;

        while (collected < chunk)
        {
            size_t bytes = 0;
            ESP_ERROR_CHECK(i2s_channel_read(
                rx_chan,
                &i2s_buf[collected],
                (chunk - collected) * sizeof(int32_t),
                &bytes,
                portMAX_DELAY));

            collected += bytes / sizeof(int32_t);
        }

        for (int i = 0; i < chunk; i++)
        {
            int32_t s = i2s_buf[i] >> 14;
            if (s > 32767)
                s = 32767;
            if (s < -32768)
                s = -32768;
            afe_buf[i] = (int16_t)s;
        }

        afe->feed(afe_data, afe_buf);
    }

    free(i2s_buf);
    free(afe_buf);
    vTaskDelete(NULL);
}

/* ================== DETECT TASK ================== */
static void detect_task(void *arg)
{
    int fetch_chunk = afe->get_fetch_chunksize(afe_data);

    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
    esp_mn_iface_t *mn = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *mn_data = mn->create(mn_name, 6000);

    ESP_LOGI(TAG, "MultiNet: %s", mn_name);
    mn->print_active_speech_commands(mn_data);

    ESP_LOGI(TAG, "---- AWAIT WAKE WORD ----");

    while (task_flag)
    {
        afe_fetch_result_t *res = afe->fetch(afe_data);
        if (!res)
            continue;

        if (res->wakeup_state == WAKENET_DETECTED)
        {
            ESP_LOGI(TAG, "WAKE WORD DETECTED");
            mn->clean(mn_data);
        }

        if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED)
        {
            ESP_LOGI(TAG, "CHANNEL VERIFIED");

            esp_mn_state_t st = mn->detect(mn_data, res->data);
            if (st == ESP_MN_STATE_DETECTED)
            {
                esp_mn_results_t *r = mn->get_results(mn_data);
                ESP_LOGI(TAG, "CMD: %s  prob=%f",
                         r->string, r->prob[0]);
            }

            if (st == ESP_MN_STATE_TIMEOUT)
            {
                ESP_LOGI(TAG, "TIMEOUT → WAIT WAKE WORD");
                afe->enable_wakenet(afe_data);
            }
        }
    }

    mn->destroy(mn_data);
    vTaskDelete(NULL);
}

/* ================== APP MAIN ================== */
void app_main(void)
{
    models = esp_srmodel_init("model");

    i2s_init();

    afe = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;

    afe_config_t cfg = AFE_CONFIG_DEFAULT();
    cfg.wakenet_model_name =
        esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);

    cfg.aec_init = false;
    cfg.se_init = false;

    cfg.pcm_config.total_ch_num = 1;
    cfg.pcm_config.mic_num = 1;
    cfg.pcm_config.ref_num = 0;

    afe_data = afe->create_from_config(&cfg);

    xTaskCreatePinnedToCore(feed_task, "feed", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(detect_task, "detect", 8192, NULL, 5, NULL, 1);
}
