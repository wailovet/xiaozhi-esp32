#include "httpd.h"
#include <esp_http_server.h>
#include "application.h"
#include <esp_log.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cJSON.h>
#include "BluetoothA2DPSource.h"
#include <nvs_flash.h>
#include <math.h>
#include "bt_audio_codec.h"
#include "mqtt_protocol.h"
#include "cc.h"
#include "assets/lang_config.h"

using namespace std;

static const char *TAG = "httpd";

std::string charToHex(unsigned char c)
{
    short i = c;

    std::stringstream s;

    s << "%" << std::setw(2) << std::setfill('0') << std::hex << i;

    return s.str();
}

unsigned char hexToChar(const std::string &str)
{
    short c = 0;

    if (!str.empty())
    {
        std::istringstream in(str);

        in >> std::hex >> c;

        if (in.fail())
        {
            throw std::runtime_error("stream decode failure");
        }
    }

    return static_cast<unsigned char>(c);
}

std::string urlEncode(const std::string &toEncode)
{
    std::ostringstream out;

    for (std::string::size_type i = 0, len = toEncode.length(); i < len; ++i)
    {
        short t = toEncode.at(i);

        if (
            t == 45 ||               // hyphen
            (t >= 48 && t <= 57) ||  // 0-9
            (t >= 65 && t <= 90) ||  // A-Z
            t == 95 ||               // underscore
            (t >= 97 && t <= 122) || // a-z
            t == 126                 // tilde
        )
        {
            out << toEncode.at(i);
        }
        else
        {
            out << charToHex(toEncode.at(i));
        }
    }

    return out.str();
}

std::string urlDecode(const std::string &toDecode)
{
    std::ostringstream out;

    for (std::string::size_type i = 0, len = toDecode.length(); i < len; ++i)
    {
        if (toDecode.at(i) == '%')
        {
            std::string str(toDecode.substr(i + 1, 2));
            out << hexToChar(str);
            i += 2;
        }
        else
        {
            out << toDecode.at(i);
        }
    }

    return out.str();
}

extern const char web_index_html_start[] asm("_binary_web_index_html_start");

BluetoothA2DPSource a2dp_source;

vector<string> bluetooth_devices;

#define NVS_CONFIG_EXT_NAME "XZEXT_ST"
string get_nvs_value_from_key(nvs_handle_t c_handle, const char *key)
{
    size_t required_size;
    nvs_get_str(c_handle, key, NULL, &required_size);
    char *value = (char *)malloc(required_size);
    nvs_get_str(c_handle, key, value, &required_size);
    string value_str(value);
    free(value);
    return value_str;
}

bool search_bt_call(const char *ssid, esp_bd_addr_t address, int rssi)
{
    ESP_LOGI(TAG, "ssid: %s, address: %02x:%02x:%02x:%02x:%02x:%02x, rssi: %d", ssid, address[0], address[1], address[2], address[3], address[4], address[5], rssi);
    for (auto i = 0; i < bluetooth_devices.size(); i++)
    {
        if (bluetooth_devices[i].find(ssid) != std::string::npos)
        {
            return false;
        }
    }

    bluetooth_devices.push_back(ssid);
    return false;
}

#define c3_frequency 130.81
int32_t bt_data_callback(Frame *frame, int32_t frame_count)
{
    ESP_LOGI(TAG, "bt_data_callback: %d", frame_count);
    static float m_time = 0.0;
    float m_amplitude = 10000.0; // -32,768 to 32,767
    float m_deltaTime = 1.0 / 44100.0;
    float m_phase = 0.0;
    float pi_2 = 3.1415 * 2.0;
    // fill the channel data
    for (int sample = 0; sample < frame_count; ++sample)
    {
        float angle = pi_2 * c3_frequency * m_time + m_phase;
        frame[sample].channel1 = m_amplitude * sin(angle);
        frame[sample].channel2 = frame[sample].channel1;
        m_time += m_deltaTime;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
    return frame_count;

    // to prevent watchdog
    // delay(1);

    ESP_LOGI(TAG, "BtAudioCodecInstance->flag = %d", BtAudioCodecInstance->flag);
    if (BtAudioCodecInstance->flag == 0)
    {
        return 0;
    }

    const int16_t *write_data = BtAudioCodecInstance->last_write_data;
    int write_samples = BtAudioCodecInstance->last_write_samples;

    for (int i = 0; i < write_samples; i++)
    {
        frame[i].channel1 = write_data[i];
        frame[i].channel2 = write_data[i];
    }

    ESP_LOGI(TAG, "bt_data_callback");

    vTaskDelay(pdMS_TO_TICKS(1));
    // return frame_count;

    BtAudioCodecInstance->flag = 0;
    return write_samples;
}

httpd_uri_t routes[] = {
    {
        .uri = "/hello",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t
        {
            httpd_resp_send(req, "Hello, World!", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        },
        .user_ctx = NULL,
    },
    {
        .uri = "/bt_lists",

        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t
        {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddItemToObject(root, "bluetooth_devices", cJSON_CreateArray());
            for (int i = 0; i < bluetooth_devices.size(); i++)
            {
                cJSON_AddItemToArray(cJSON_GetObjectItem(root, "bluetooth_devices"), cJSON_CreateString(bluetooth_devices[i].c_str()));
            }
            httpd_resp_set_type(req, "application/json");

            httpd_resp_send(req, cJSON_PrintUnformatted(root), HTTPD_RESP_USE_STRLEN);

            cJSON_Delete(root);
            return ESP_OK;
        },
        .user_ctx = NULL,
    },
    {
        .uri = "/bt_search",

        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t
        {
            httpd_resp_send(req, "1", HTTPD_RESP_USE_STRLEN);
            a2dp_source.set_ssid_callback(search_bt_call);
            a2dp_source.start();
            vTaskDelay(pdMS_TO_TICKS(10 * 1000));
            a2dp_source.end(true);
            return ESP_OK;
        },
        .user_ctx = NULL,
    },

    // {
    //     .uri = "/bt_a2dp_source",

    //     .method = HTTP_GET,
    //     .handler = [](httpd_req_t *req) -> esp_err_t
    //     {
    //         auto audio_state = a2dp_source.get_audio_state();

    //         // ESP_A2D_AUDIO_STATE_SUSPEND = 0,           /*!< audio stream datapath suspended by remote device */
    //         // ESP_A2D_AUDIO_STATE_STARTED,               /*!< audio stream datapath started */
    //         // ESP_A2D_AUDIO_STATE_STOPPED = ESP_A2D_AUDIO_STATE_SUSPEND,          /*!< @note Deprecated */
    //         // ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND = ESP_A2D_AUDIO_STATE_SUSPEND,   /*!< @note Deprecated */
    //         char *audio_state_map[] = {
    //             "ESP_A2D_AUDIO_STATE_SUSPEND",
    //             "ESP_A2D_AUDIO_STATE_STARTED",
    //             "ESP_A2D_AUDIO_STATE_STOPPED",
    //             "ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND",
    //         };
    //         auto connection_state = a2dp_source.get_connection_state();

    //         // ESP_A2D_CONNECTION_STATE_DISCONNECTED = 0, /*!< connection released  */
    //         // ESP_A2D_CONNECTION_STATE_CONNECTING,       /*!< connecting remote device */
    //         // ESP_A2D_CONNECTION_STATE_CONNECTED,        /*!< connection established */
    //         // ESP_A2D_CONNECTION_STATE_DISCONNECTING     /*!< disconnecting remote device */
    //         char *connection_state_map[] = {
    //             "ESP_A2D_CONNECTION_STATE_DISCONNECTED",
    //             "ESP_A2D_CONNECTION_STATE_CONNECTING",
    //             "ESP_A2D_CONNECTION_STATE_CONNECTED",
    //             "ESP_A2D_CONNECTION_STATE_DISCONNECTING",
    //         };

    //         auto volume = a2dp_source.get_volume();
    //         //  uint8_t *last_peer_address = a2dp_source.get_last_peer_address();
    //         auto name = a2dp_source.get_name();
    //         cJSON *root = cJSON_CreateObject();
    //         cJSON_AddItemToObject(root, "audio_state", cJSON_CreateNumber(audio_state));
    //         cJSON_AddItemToObject(root, "audio_state_text", cJSON_CreateString(audio_state_map[audio_state]));
    //         cJSON_AddItemToObject(root, "connection_state", cJSON_CreateNumber(connection_state));
    //         cJSON_AddItemToObject(root, "connection_state_text", cJSON_CreateString(connection_state_map[connection_state]));
    //         cJSON_AddItemToObject(root, "volume", cJSON_CreateNumber(volume));
    //         //  cJSON_AddItemToObject(root, "last_peer_address", cJSON_CreateString(last_peer_address));
    //         cJSON_AddItemToObject(root, "name", cJSON_CreateString(name));
    //         httpd_resp_set_type(req, "application/json");
    //         httpd_resp_send(req, cJSON_PrintUnformatted(root), HTTPD_RESP_USE_STRLEN);
    //         return ESP_OK;
    //     },
    //     .user_ctx = NULL,
    // },
    {
        .uri = "/restart",

        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t
        {
            httpd_resp_send(req, "1", HTTPD_RESP_USE_STRLEN);

            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
            return ESP_OK;
        },
        .user_ctx = NULL,
    },
    {
        .uri = "/set_nvs",

        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t
        {
            std::string uri = req->uri;
            ESP_LOGI(TAG, "uri: %s", uri.c_str());
            auto pos = uri.find("?json=");
            char *key = {};
            char *value = {};
            if (pos != std::string::npos)
            {
                auto json_str = uri.substr(pos + 6);
                cJSON *root;
                ESP_LOGI(TAG, "json_str: %s", json_str.c_str());
                string json_str_decode = urlDecode(json_str);
                ESP_LOGI(TAG, "json_str_decode: %s", json_str_decode.c_str());
                root = cJSON_Parse(json_str_decode.c_str());
                key = cJSON_GetObjectItem(root, "key")->valuestring;
                value = cJSON_GetObjectItem(root, "value")->valuestring;

                nvs_handle_t nvs_handle;
                ESP_ERROR_CHECK(nvs_open(NVS_CONFIG_EXT_NAME, NVS_READWRITE, &nvs_handle));

                ESP_LOGD(TAG, "key: %s, value: %s", key, value);

                nvs_set_str(nvs_handle, key, value);

                nvs_commit(nvs_handle);
                nvs_close(nvs_handle);
                cJSON_Delete(root);
            }

            httpd_resp_send(req, uri.c_str(), HTTPD_RESP_USE_STRLEN);

            return ESP_OK;
        },
        .user_ctx = NULL,
    },
    {
        .uri = "/get_nvs_all",
        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t
        {
            nvs_iterator_t it = NULL;
            nvs_handle_t nvs_handle;
            ESP_ERROR_CHECK(nvs_open(NVS_CONFIG_EXT_NAME, NVS_READWRITE, &nvs_handle));
            esp_err_t res = nvs_entry_find_in_handle(nvs_handle, NVS_TYPE_ANY, &it);

            cJSON *root = cJSON_CreateObject();
            while (res == ESP_OK)
            {
                nvs_entry_info_t info;
                nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
                // printf("key '%s', type '%d' \n", info.key, info.type);
                ESP_LOGI(TAG, "key: %s, type: %d", info.key, info.type);
                if (info.type == NVS_TYPE_STR)
                {
                    string value_buffer = get_nvs_value_from_key(nvs_handle, info.key);
                    cJSON_AddItemToObject(root, info.key, cJSON_CreateString(value_buffer.c_str()));
                }
                res = nvs_entry_next(&it);
            }

            httpd_resp_set_type(req, "application/json");
            nvs_close(nvs_handle);
            httpd_resp_send(req, cJSON_PrintUnformatted(root), HTTPD_RESP_USE_STRLEN);
            cJSON_Delete(root);

            return ESP_OK;
        },
        .user_ctx = NULL,
    },
    // {
    //     .uri = "/test_player_audio",

    //     .method = HTTP_GET,
    //     .handler = [](httpd_req_t *req) -> esp_err_t
    //     {
    //         httpd_resp_send(req, "2", HTTPD_RESP_USE_STRLEN);
    //         ESP_LOGI(TAG, "test_player_audio");
    //         // a2dp_source.set_ssid_callback(search_bt_call);
    //         a2dp_source.set_data_callback_in_frames(bt_data_callback);
    //         a2dp_source.start("BT蓝牙音箱");
    //         // a2dp_source.start("DESKTOP-0L2K9C7");
    //         return ESP_OK;
    //     },
    //     .user_ctx = NULL,
    // },
    {
        .uri = "/start_listening",

        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t
        {
            httpd_resp_send(req, "2", HTTPD_RESP_USE_STRLEN);
            Application::GetInstance().StartListening();
            return ESP_OK;
        },
        .user_ctx = NULL,
    },
    {
        .uri = "/stop_listening",

        .method = HTTP_GET,
        .handler = [](httpd_req_t *req) -> esp_err_t
        {
            httpd_resp_send(req, "2", HTTPD_RESP_USE_STRLEN);
            Application::GetInstance().StopListening();
            return ESP_OK;
        },
        .user_ctx = NULL,
    },

};

static httpd_handle_t server = NULL;
void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 58080;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_LOGI(TAG, "Registering URI handlers");
        // httpd_register_uri_handler(server, &hello);
        for (int i = 0; i < sizeof(routes) / sizeof(httpd_uri_t); i++)
        {
            httpd_register_uri_handler(server, &routes[i]);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Error starting server!");
    }
}

void stop_webserver(void)
{
    if (server)
    {
        httpd_stop(server);
        ESP_LOGI(TAG, "Server stopped");
    }
}