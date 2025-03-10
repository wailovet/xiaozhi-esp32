#include "wifi_board.h"
#include "ml307_board.h"
#include "system_reset.h"
#include "audio_codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "display/ssd1306_display.h"
#include "display/no_display.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

#include "esp_system.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_hf_ag_api.h"
#include "esp_gap_bt_api.h"
#include "esp_console.h"
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "httpd.h"

#define TAG "ESP32-SimpleSupport"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

/* event for handler "bt_av_hdl_stack_up */
enum
{
    BT_APP_EVT_STACK_UP = 0,
};

static char *bda2str(esp_bd_addr_t bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18)
    {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

char bda_str[18] = {0};

class EspSimpleMl307Board : public Ml307Board
{
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Button boot_button_;
    Button touch_button_;

    void InitializeDisplayI2c()
    {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             { Application::GetInstance().ToggleChatState(); });
        touch_button_.OnPressDown([this]()
                                  { Application::GetInstance().StartListening(); });
        touch_button_.OnPressUp([this]()
                                { Application::GetInstance().StopListening(); });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot()
    {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

public:
    EspSimpleMl307Board() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN, 4096),
                            boot_button_(BOOT_BUTTON_GPIO),
                            touch_button_(TOUCH_BUTTON_GPIO)
    {

        InitializeDisplayI2c();
        InitializeButtons();
        // InitializeIot();
    }

    virtual Led *GetLed() override
    {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                               AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif

        // static BtAudioCodec *audio_codec = new BtAudioCodec(AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                      &font_puhui_14_1, &font_awesome_14_1);
        return &display;
    }
};

class EspSimpleWifiBoard : public WifiBoard
{
private:
    Button boot_button_;
    Button touch_button_;
    Button asr_button_;
    NoDisplay *display_;

    // BtAudioCodec *audio_codec = new BtAudioCodec(AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);

    NoAudioCodecDuplex *audio_codec = new NoAudioCodecDuplex(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
    i2c_master_bus_handle_t display_i2c_bus_;

    void InitializeDisplayI2c()
    {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }
    void InitializeNoDisplay()
    {
        display_ = new NoDisplay();
    }

    void InitializeButtons()
    {

        // 配置 GPIO
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO, // 设置需要配置的 GPIO 引脚
            .mode = GPIO_MODE_OUTPUT,                 // 设置为输出模式
            .pull_up_en = GPIO_PULLUP_DISABLE,        // 禁用上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE,    // 禁用下拉
            .intr_type = GPIO_INTR_DISABLE            // 禁用中断
        };
        gpio_config(&io_conf); // 应用配置

        boot_button_.OnClick([this]()
                             {
            ESP_LOGI(TAG, "boot button clicked");                   
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            gpio_set_level(BUILTIN_LED_GPIO, 1);
            app.ToggleChatState(); });

        asr_button_.OnClick([this]()
                            {
            ESP_LOGI(TAG, "asr button clicked");
            std::string wake_word="你好小智";
            Application::GetInstance().WakeWordInvoke(wake_word); });

        touch_button_.OnPressDown([this]()
                                  {
            gpio_set_level(BUILTIN_LED_GPIO, 1);
            Application::GetInstance().StartListening(); });
        touch_button_.OnPressUp([this]()
                                {
            gpio_set_level(BUILTIN_LED_GPIO, 0);
            Application::GetInstance().StopListening(); });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot()
    {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

public:
    EspSimpleWifiBoard() : boot_button_(BOOT_BUTTON_GPIO), touch_button_(TOUCH_BUTTON_GPIO), asr_button_(ASR_BUTTON_GPIO)
    {

        this->httpdServerStartUpCallback = start_webserver;
        InitializeDisplayI2c();
        InitializeNoDisplay();
        InitializeButtons();

        // start_webserver();
        // return ;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
// #ifdef AUDIO_I2S_METHOD_SIMPLEX
//         static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
//                                                AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
// #else
//         static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
//                                               AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
// #endif
        // ESP_LOGI(TAG, "GetAudioCodec AUDIO_INPUT_SAMPLE_RATE %d AUDIO_OUTPUT_SAMPLE_RATE %d", AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE);
        // ESP_LOGI(TAG, "GetAudioCodec AUDIO_I2S_GPIO_BCLK %d AUDIO_I2S_GPIO_WS %d AUDIO_I2S_GPIO_DOUT %d AUDIO_I2S_GPIO_DIN %d", AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return audio_codec;
    }

    // virtual Display* GetDisplay() override {
    //     static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
    //                                 &font_puhui_14_1, &font_awesome_14_1);
    //     return &display;
    // }
    virtual Display *GetDisplay() override
    {
        return display_;
    }
};

DECLARE_BOARD(EspSimpleWifiBoard);
