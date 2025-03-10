#include "bt_audio_codec.h"

#include <esp_log.h>
// #include "AudioTools.h"
// #include "BluetoothA2DPSource.h"
// #include "AudioTools/AudioCodecs/CodecWAV.h"
// #include "mbedtls/base64.h"
#include <string.h>
#include <math.h>

#define c3_frequency 130.81

static const char TAG[] = "BtAudioCodec";

BtAudioCodec::BtAudioCodec(gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din)
{
    ESP_LOGI(TAG, "BtAudioCodec initialized");

    duplex_ = false;
    input_sample_rate_ = 16000;
    output_sample_rate_ = 24000;
    // Create a new channel for speaker
    i2s_chan_config_t chan_cfg = {
        .id = (i2s_port_t)0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    //     ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, nullptr));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
#ifdef I2S_HW_VERSION_2
            .ext_clk_freq_hz = 0,
#endif

        },
        .slot_cfg = {.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT, .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, .slot_mode = I2S_SLOT_MODE_MONO, .slot_mask = I2S_STD_SLOT_LEFT, .ws_width = I2S_DATA_BIT_WIDTH_32BIT, .ws_pol = false, .bit_shift = true,
#ifdef I2S_HW_VERSION_2
                     .left_align = true,
                     .big_endian = false,
                     .bit_order_lsb = false
#endif

        },
        .gpio_cfg = {.mclk = I2S_GPIO_UNUSED, .bclk = spk_bclk, .ws = spk_ws, .dout = spk_dout, .din = I2S_GPIO_UNUSED, .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};
    //     ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));

    //     // Create a new channel for MIC
    chan_cfg.id = (i2s_port_t)1;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_handle_));
    std_cfg.clk_cfg.sample_rate_hz = (uint32_t)input_sample_rate_;
    std_cfg.gpio_cfg.bclk = mic_sck;
    std_cfg.gpio_cfg.ws = mic_ws;
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din = mic_din;
    // ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
    ESP_LOGI(TAG, "Simplex channels created");

    BtAudioCodecInstance = this;
}

BtAudioCodec::~BtAudioCodec()
{
    ESP_LOGI(TAG, "BtAudioCodec destroyed");
}

void BtAudioCodec::CreateVoiceHardware()
{

    ESP_LOGI(TAG, "Voice hardware created");
}

void BtAudioCodec::SetOutputVolume(int volume)
{
    ESP_LOGI(TAG, "Set output volume to %d", volume);
}

void BtAudioCodec::EnableInput(bool enable)
{
    ESP_LOGI(TAG, "Enable input %d", enable);
}

void BtAudioCodec::EnableOutput(bool enable)
{
    ESP_LOGI(TAG, "Enable output %d", enable);
}

int BtAudioCodec::Read(int16_t *dest, int samples)
{
    ESP_LOGI(TAG, "Read %d samples", samples);
    return samples;
}

void BtAudioCodec::Start()
{
    ESP_LOGI(TAG, "Start");
}
int BtAudioCodec::Write(const int16_t *data, int samples)
{
    last_write_data = data;
    last_write_samples = samples;

    flag = 1;
    ESP_LOGI(TAG, "Write %d samples", samples);
    while (flag == 1)
    {
        vTaskDelay(1);
    }
    ESP_LOGI(TAG, "Finished Write %d current_sample_rate", current_sample_rate);

    return current_sample_rate;
}
