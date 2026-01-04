/**
 * @file audio_feed.c
 * @brief Audio feed module implementation
 */

#include "audio_feed.h"
#include "app_config.h"
#include "esp_log.h"
#include <stdlib.h>
#include <assert.h>

static const char *TAG = "[AUDIO]";

/* ================== GLOBAL VARIABLES ================== */
esp_afe_sr_iface_t *g_afe = NULL;
esp_afe_sr_data_t *g_afe_data = NULL;
i2s_chan_handle_t g_rx_chan = NULL;
volatile int g_task_flag = 1;

/* ================== PUBLIC FUNCTIONS ================== */
esp_err_t audio_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        I2S_NUM_0, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &g_rx_chan));

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

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(g_rx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(g_rx_chan));

    ESP_LOGI(TAG, "I2S init OK");
    return ESP_OK;
}

void feed_task(void *arg)
{
    int chunk = g_afe->get_feed_chunksize(g_afe_data);

    int32_t *i2s_buf = malloc(chunk * sizeof(int32_t));
    int16_t *afe_buf = malloc(chunk * sizeof(int16_t));
    assert(i2s_buf && afe_buf);

    ESP_LOGI(TAG, "AFE feed chunk = %d", chunk);

    while (g_task_flag)
    {
        int collected = 0;

        while (collected < chunk)
        {
            size_t bytes = 0;
            ESP_ERROR_CHECK(i2s_channel_read(
                g_rx_chan,
                &i2s_buf[collected],
                (chunk - collected) * sizeof(int32_t),
                &bytes,
                portMAX_DELAY));

            collected += bytes / sizeof(int32_t);
        }

        // Convert 32-bit to 16-bit audio
        for (int i = 0; i < chunk; i++)
        {
            int32_t s = i2s_buf[i] >> 14;
            if (s > 32767)
                s = 32767;
            if (s < -32768)
                s = -32768;
            afe_buf[i] = (int16_t)s;
        }

        g_afe->feed(g_afe_data, afe_buf);
    }

    free(i2s_buf);
    free(afe_buf);
    ESP_LOGI(TAG, "Feed task exit");
    vTaskDelete(NULL);
}
