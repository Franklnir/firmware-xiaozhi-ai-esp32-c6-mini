#include "application.h"
#include "button.h"
#include "codecs/no_audio_codec.h"
#include "config.h"
#include "supermini_lcd_display.h"
#include "backlight.h"
#include <driver/spi_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include "led/single_led.h"
#include "mcp_server.h"
#include "wifi_board.h"

#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <ssid_manager.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>
#include "settings.h"

#define TAG "Esp32C6SuperMini"

class Esp32C6SuperMiniBoard : public WifiBoard {
private:
    Button boot_button_;
    Button chat_button_;
    Display* display_ = nullptr;
    esp_timer_handle_t status_led_timer_ = nullptr;
    bool last_charge_state_valid_ = false;
    bool last_charging_ = false;
    bool display_rotated_ = false;

    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Backlight* backlight_ = nullptr;

    virtual Backlight* GetBacklight() override {
        return backlight_;
    }

    static uint64_t GpioBit(gpio_num_t gpio) {
        return 1ULL << static_cast<uint32_t>(gpio);
    }

    void SetBuiltinStatusLed(bool on) {
        if (BUILTIN_LED_GPIO == GPIO_NUM_NC) {
            return;
        }
        gpio_set_level(BUILTIN_LED_GPIO, on ? BUILTIN_LED_ACTIVE_LEVEL : !BUILTIN_LED_ACTIVE_LEVEL);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCL_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSt7789Display() {
        ESP_LOGI(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 10 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RES_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        
        // Manual hardware reset to wake up stubborn displays
        gpio_set_direction(DISPLAY_RES_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(DISPLAY_RES_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(DISPLAY_RES_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(150));

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));

        display_ = new SuperminiLcdDisplay(panel_io_, panel_,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        
        if (DISPLAY_BLK_PIN != GPIO_NUM_NC) {
            backlight_ = new PwmBacklight(DISPLAY_BLK_PIN, false);
            backlight_->RestoreBrightness();
        }
    }

    void InitializePowerStatusLed() {
        if (BUILTIN_LED_GPIO == GPIO_NUM_NC) {
            return;
        }

        gpio_config_t led_config = {};
        led_config.intr_type = GPIO_INTR_DISABLE;
        led_config.mode = GPIO_MODE_OUTPUT;
        led_config.pin_bit_mask = GpioBit(BUILTIN_LED_GPIO);
        led_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        led_config.pull_up_en = GPIO_PULLUP_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&led_config));

        SetBuiltinStatusLed(true);
    }

    bool IsCharging() {
        if (BATTERY_CHARGING_GPIO == GPIO_NUM_NC) {
            return false;
        }
        return gpio_get_level(BATTERY_CHARGING_GPIO) == BATTERY_CHARGING_ACTIVE_LEVEL;
    }

    void UpdateStatusLed() {
        auto state = Application::GetInstance().GetDeviceState();
        bool is_disconnected = (state == kDeviceStateWifiConfiguring || state == kDeviceStateStarting);
        
        static bool led_blink_state = false;
        
        if (is_disconnected) {
            led_blink_state = !led_blink_state;
            SetBuiltinStatusLed(led_blink_state);
        } else {
            if (BATTERY_CHARGING_GPIO != GPIO_NUM_NC) {
                bool charging = IsCharging();
                SetBuiltinStatusLed(charging);
                
                if (!last_charge_state_valid_ || charging != last_charging_) {
                    last_charge_state_valid_ = true;
                    last_charging_ = charging;
                    ESP_LOGI(TAG, "Battery charge status: %s", charging ? "charging" : "not charging");
                }
            } else {
                SetBuiltinStatusLed(false);
            }
        }
    }

    void InitializeStatusLedTimer() {
        if (BATTERY_CHARGING_GPIO != GPIO_NUM_NC) {
            gpio_config_t charge_config = {};
            charge_config.intr_type = GPIO_INTR_DISABLE;
            charge_config.mode = GPIO_MODE_INPUT;
            charge_config.pin_bit_mask = GpioBit(BATTERY_CHARGING_GPIO);
            charge_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
            charge_config.pull_up_en = GPIO_PULLUP_ENABLE;
            ESP_ERROR_CHECK(gpio_config(&charge_config));
            ESP_LOGI(TAG, "Battery charge-status GPIO%d mirrors charging state to GPIO%d", BATTERY_CHARGING_GPIO, BUILTIN_LED_GPIO);
        }

        esp_timer_create_args_t timer_args = {};
        timer_args.callback = [](void* arg) {
            static_cast<Esp32C6SuperMiniBoard*>(arg)->UpdateStatusLed();
        };
        timer_args.arg = this;
        timer_args.name = "status_led";
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &status_led_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(status_led_timer_, 500 * 1000));

        UpdateStatusLed();
    }

    bool IsDeepSleepWakeGpioConfigured() {
        return DEEP_SLEEP_WAKE_GPIO != GPIO_NUM_NC;
    }

    void EnterSoftPowerOff() {
        if (!IsDeepSleepWakeGpioConfigured()) {
            ESP_LOGW(TAG, "Soft power-off is disabled. Set DEEP_SLEEP_WAKE_GPIO to GPIO0..GPIO7 and wire a wake button");
            return;
        }

        gpio_config_t wake_config = {};
        wake_config.intr_type = GPIO_INTR_DISABLE;
        wake_config.mode = GPIO_MODE_INPUT;
        wake_config.pin_bit_mask = GpioBit(DEEP_SLEEP_WAKE_GPIO);
        wake_config.pull_down_en = DEEP_SLEEP_WAKE_ACTIVE_LEVEL ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
        wake_config.pull_up_en = DEEP_SLEEP_WAKE_ACTIVE_LEVEL ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE;
        ESP_ERROR_CHECK(gpio_config(&wake_config));

        auto wake_mode = DEEP_SLEEP_WAKE_ACTIVE_LEVEL ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW;
        esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(GpioBit(DEEP_SLEEP_WAKE_GPIO), wake_mode);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "GPIO%d cannot wake ESP32-C6 from deep sleep: %s",
                DEEP_SLEEP_WAKE_GPIO, esp_err_to_name(err));
            return;
        }

        if (status_led_timer_ != nullptr) {
            esp_timer_stop(status_led_timer_);
        }
        SetBuiltinStatusLed(false);
        ESP_LOGI(TAG, "Entering soft power-off. Wake source: GPIO%d", DEEP_SLEEP_WAKE_GPIO);
        esp_deep_sleep_start();
    }

    void InitializeRelays() {
        if (RELAY_CH1_GPIO != GPIO_NUM_NC) {
            gpio_config_t relay_config = {};
            relay_config.intr_type = GPIO_INTR_DISABLE;
            relay_config.mode = GPIO_MODE_OUTPUT;
            relay_config.pin_bit_mask = GpioBit(RELAY_CH1_GPIO) | GpioBit(RELAY_CH2_GPIO);
            relay_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
            relay_config.pull_up_en = GPIO_PULLUP_DISABLE;
            ESP_ERROR_CHECK(gpio_config(&relay_config));

            Settings settings("relay", false);
            bool ch1_state = settings.GetBool("ch1", false);
            bool ch2_state = settings.GetBool("ch2", false);

            gpio_set_level(RELAY_CH1_GPIO, ch1_state ? 0 : 1);
            gpio_set_level(RELAY_CH2_GPIO, ch2_state ? 0 : 1);
            ESP_LOGI(TAG, "Relays initialized. CH1: %s, CH2: %s", ch1_state ? "ON" : "OFF", ch2_state ? "ON" : "OFF");
        }
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnLongPress([this]() {
            EnterWifiConfigMode();
        });

        if (CHAT_BUTTON_GPIO != GPIO_NUM_NC) {
            chat_button_.OnClick([this]() {
                auto& app = Application::GetInstance();
                app.ToggleChatState();
            });

            chat_button_.OnDoubleClick([this]() {
                ESP_LOGI(TAG, "Idle smartwatch themes disabled for stability.");
            });

            // Triple click: rotasi layar 180°
            chat_button_.OnMultipleClick([this]() {
                display_rotated_ = !display_rotated_;
                bool mx = DISPLAY_MIRROR_X ^ display_rotated_;
                bool my = DISPLAY_MIRROR_Y ^ display_rotated_;
                ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, mx, my));

                // Paksa LVGL menggambar ulang seluruh layar
                if (lvgl_port_lock(200)) {
                    lv_obj_invalidate(lv_scr_act());
                    lvgl_port_unlock();
                }

                // Simpan status rotasi ke NVS
                Settings settings("display", true);
                settings.SetBool("rotated", display_rotated_);

                ESP_LOGI(TAG, "Display rotated: %s (saved)", display_rotated_ ? "180°" : "0°");
            }, 3);

            // Long press 6 detik: masuk mode konfigurasi WiFi
            chat_button_.OnLongPress([this]() {
                EnterWifiConfigMode();
            });
        }
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.system.reconfigure_wifi",
            "End this conversation and enter WiFi configuration mode. "
            "Use this for voice commands such as 'reset wifi', 'atur ulang wifi', "
            "'hapus wifi lama', 'ganti wifi', 'konfigurasi ulang wifi', or 'masuk mode wifi'. "
            "Set `forget_saved_networks` to true only when the user explicitly asks to reset, forget, "
            "or delete saved WiFi credentials. For changing or reconfiguring WiFi, keep it false.\n"
            "**CAUTION** You must ask the user to confirm this action.",
            PropertyList({
                Property("forget_saved_networks", kPropertyTypeBoolean, false)
            }), [this](const PropertyList& properties) -> ReturnValue {
                if (properties["forget_saved_networks"].value<bool>()) {
                    SsidManager::GetInstance().Clear();
                    ESP_LOGW(TAG, "Saved WiFi credentials cleared by voice command");
                }
                EnterWifiConfigMode();
                return true;
            });

        mcp_server.AddTool("self.battery.get_status",
            "Get battery charging status. Use this for voice commands such as "
            "'status baterai', 'apakah sedang charging', 'lagi dicas atau tidak', "
            "or 'cek charging'. The result reports charging or not_charging. "
            "Battery percentage is unavailable unless a battery ADC divider is added.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                cJSON* result = cJSON_CreateObject();
                bool configured = BATTERY_CHARGING_GPIO != GPIO_NUM_NC;
                bool charging = configured && IsCharging();
                cJSON_AddBoolToObject(result, "configured", configured);
                cJSON_AddNumberToObject(result, "charging_gpio", configured ? BATTERY_CHARGING_GPIO : -1);
                cJSON_AddBoolToObject(result, "charging", charging);
                cJSON_AddBoolToObject(result, "discharging", configured && !charging);
                cJSON_AddStringToObject(result, "status", configured ? (charging ? "charging" : "not_charging") : "not_configured");
                cJSON_AddBoolToObject(result, "battery_level_available", false);
                return result;
            });

        mcp_server.AddTool("self.relay.set_channel",
            "Mengatur state dari relay 2 channel. Gunakan untuk perintah suara seperti "
            "'nyalakan relay 1', 'matikan lampu 2', dll.",
            PropertyList({
                Property("channel", kPropertyTypeInteger, 1, 2),
                Property("state", kPropertyTypeBoolean)
            }), [this](const PropertyList& properties) -> ReturnValue {
                int channel = properties["channel"].value<int>();
                bool state = properties["state"].value<bool>();

                Settings settings("relay", true);
                if (channel == 1) {
                    gpio_set_level(RELAY_CH1_GPIO, state ? 0 : 1);
                    settings.SetBool("ch1", state);
                    ESP_LOGI(TAG, "Relay CH1 set to %s", state ? "ON" : "OFF");
                } else if (channel == 2) {
                    gpio_set_level(RELAY_CH2_GPIO, state ? 0 : 1);
                    settings.SetBool("ch2", state);
                    ESP_LOGI(TAG, "Relay CH2 set to %s", state ? "ON" : "OFF");
                }
                
                return true;
            });

        mcp_server.AddTool("self.relay.get_status",
            "Mendapatkan status relay saat ini. Gunakan untuk menjawab pertanyaan seperti "
            "'apakah relay 1 menyala?', 'status lampu 2', dll.",
            PropertyList(), [this](const PropertyList& properties) -> ReturnValue {
                Settings settings("relay", false);
                bool ch1_state = settings.GetBool("ch1", false);
                bool ch2_state = settings.GetBool("ch2", false);

                cJSON* result = cJSON_CreateObject();
                cJSON_AddBoolToObject(result, "relay_1", ch1_state);
                cJSON_AddBoolToObject(result, "relay_2", ch2_state);
                return result;
            });
    }

public:
    Esp32C6SuperMiniBoard() : boot_button_(BOOT_BUTTON_GPIO, false, 3000), chat_button_(CHAT_BUTTON_GPIO, false, 6000) {
        InitializePowerStatusLed();
        InitializeStatusLedTimer();
        InitializeRelays();
        InitializeSpi();
        InitializeSt7789Display();

        // Muat status rotasi terakhir dari NVS
        {
            Settings settings("display", false);
            display_rotated_ = settings.GetBool("rotated", false);
            if (display_rotated_) {
                bool mx = DISPLAY_MIRROR_X ^ true;
                bool my = DISPLAY_MIRROR_Y ^ true;
                ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, mx, my));
                ESP_LOGI(TAG, "Restored display rotation: 180°");
            }
        }

        InitializeButtons();
        InitializeTools();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecDuplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    virtual Led* GetLed() override {
        static SingleLed led(RGB_LED_GPIO);
        return &led;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (BATTERY_CHARGING_GPIO == GPIO_NUM_NC) {
            return false;
        }
        charging = IsCharging();
        discharging = !charging;
        level = 100;
        return true;
    }
};

DECLARE_BOARD(Esp32C6SuperMiniBoard);
