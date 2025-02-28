#include "bt_audio_codec.h"

#include <esp_log.h>
#include <driver/i2c.h>
#include <driver/i2c_master.h>
#include <driver/i2s_tdm.h>

static const char TAG[] = "BtAudioCodec";

BtAudioCodec::BtAudioCodec()
{

    ESP_LOGI(TAG, "BtAudioCodec initialized");
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
    AudioCodec::EnableInput(enable);
    
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

void AdjustVolume(const int16_t *input_data, int16_t *output_data, size_t samples, float volume)
{
    ESP_LOGI(TAG, "Adjust volume %f", volume);
}

int BtAudioCodec::Write(const int16_t *data, int samples)
{
    ESP_LOGI(TAG, "Write %d samples", samples);
    return samples;
}
