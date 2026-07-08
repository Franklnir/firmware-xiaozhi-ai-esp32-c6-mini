#ifndef SUPERMINI_LCD_DISPLAY_H
#define SUPERMINI_LCD_DISPLAY_H

#include "display/lcd_display.h"
#include "lvgl_theme.h"
#include "board.h"
#include "config.h"
#include "assets/lang_config.h"
#include "idle_clock_background.h"
#include "idle_clock_faces.h"
#include "watchbyirsyad2.h"
#include "settings.h"

#include <cJSON.h>
#include <esp_lvgl_port.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <wifi_manager.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <ctime>
#include <string>
#include <utility>

LV_FONT_DECLARE(font_puhui_basic_30_4);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_24);
LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(lv_font_montserrat_48);

class SuperminiLcdDisplay : public SpiLcdDisplay {
protected:
    static constexpr int kIdleCanvasWidth = 240;
    static constexpr int kIdleCanvasHeight = 280;
    static constexpr int kWaveBgSize = 200;
    static constexpr int kIdleBaseX = 0;
    static constexpr int kIdleBaseY = 0;
    static constexpr int kWatchCenterX = 120;
    static constexpr int kWatchCenterY = 140;
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr int32_t kDefaultWeatherLatitudeE6 = -6200000;
    static constexpr int32_t kDefaultWeatherLongitudeE6 = 106816700;
    static constexpr int64_t kWeatherRefreshIntervalMs = 30 * 60 * 1000;
    static constexpr int64_t kWeatherRetryIntervalMs = 5 * 60 * 1000;

    lv_obj_t* high_temp_popup_ = nullptr;
    lv_obj_t* high_temp_label_ = nullptr;
    lv_obj_t* idle_clock_layer_ = nullptr;
    lv_obj_t* idle_canvas_ = nullptr;
    lv_obj_t* idle_wave_overlay_ = nullptr;
    lv_obj_t* idle_watch_overlay_ = nullptr;
    lv_obj_t* idle_wifi_clock_overlay_ = nullptr;
    lv_obj_t* idle_watchesp_overlay_ = nullptr;
    lv_obj_t* idle_time_label_ = nullptr;
    lv_obj_t* idle_date_label_ = nullptr;
    lv_obj_t* idle_weekday_label_ = nullptr;
    lv_obj_t* idle_temperature_label_ = nullptr;
    lv_obj_t* idle_wake_label_ = nullptr;
    lv_obj_t* wifi_clock_time_label_ = nullptr;
    lv_obj_t* wifi_clock_date_label_ = nullptr;
    lv_obj_t* wifi_clock_status_label_ = nullptr;
    lv_obj_t* watchesp_ring_labels_[12] = {};
    lv_obj_t* watchesp_second_label_ = nullptr;
    lv_obj_t* watchesp_hour_minute_label_ = nullptr;
    lv_obj_t* watchesp_weekday_label_ = nullptr;
    lv_obj_t* watchesp_month_digits_[2] = {};
    lv_obj_t* watchesp_day_digits_[2] = {};
    lv_obj_t* watchbyirsyad1_hour_label_ = nullptr;
    lv_obj_t* watchbyirsyad1_minute_label_ = nullptr;
    lv_obj_t* watchbyirsyad1_day_label_ = nullptr;
    lv_obj_t* watchbyirsyad1_date_label_ = nullptr;
    lv_obj_t* watchbyirsyad1_month_label_ = nullptr;
    lv_obj_t* watchbyirsyad1_year_label_ = nullptr;
    lv_obj_t* watchbyirsyad1_weather_label_ = nullptr;
    lv_obj_t* watchbyirsyad2_weather_label_ = nullptr;
    lv_obj_t* watchbyirsyad2_weekday_label_ = nullptr;
    lv_obj_t* watchbyirsyad2_date_label_ = nullptr;
    lv_obj_t* idle_battery_segments_[5] = {};
    lv_obj_t* wifi_clock_bars_[4] = {};
    lv_obj_t* analog_hour_line_ = nullptr;
    lv_obj_t* analog_minute_line_ = nullptr;
    lv_obj_t* analog_second_line_ = nullptr;
    lv_obj_t* analog_center_dot_ = nullptr;
    lv_point_precise_t analog_hour_points_[2] = {};
    lv_point_precise_t analog_minute_points_[2] = {};
    lv_point_precise_t analog_second_points_[2] = {};
    void* idle_background_buffer_ = nullptr;
    uint32_t idle_background_stride_ = 0;
    std::string rendered_idle_face_;
    bool idle_clock_visible_ = false;
    std::mutex weather_mutex_;
    std::string weather_temperature_text_ = "--C";
    std::string weather_condition_text_ = "";
    std::string weather_location_text_ = "Jakarta";
    int64_t weather_last_success_ms_ = 0;
    int64_t weather_last_attempt_ms_ = 0;
    bool weather_fetch_in_progress_ = false;

    struct WeatherSnapshot {
        std::string temperature;
        std::string condition;
        std::string location;
        bool valid = false;
    };

    static bool IsTimeStatus(const char* status) {
        if (status == nullptr || std::strlen(status) != 5 || status[2] != ':') {
            return false;
        }
        return status[0] >= '0' && status[0] <= '9' &&
               status[1] >= '0' && status[1] <= '9' &&
               status[3] >= '0' && status[3] <= '9' &&
               status[4] >= '0' && status[4] <= '9';
    }

    static bool IsIdleStatus(const char* status) {
#ifdef CONFIG_BOARD_TYPE_ESP32_C6_SUPER_MINI
        return false;
#else
        return IsTimeStatus(status);
#endif
    }

    static const unsigned char* GetWatchFaceData(const std::string& face) {
        if (face == "smartq02") {
            return smartq02_face;
        }
        if (face == "smartq04") {
            return smartq04_face;
        }
        if (face == "smartq07") {
            return smartq07_face;
        }
        if (face == "smartq08") {
            return smartq08_face;
        }
        if (face == "smartq09") {
            return smartq09_face;
        }
        return nullptr;
    }

    static const uint16_t* GetRawWatchFaceData(const std::string& face) {
        if (face == "watchbyirsyad2") {
            return epd_bitmap_watchbyirsyad2;
        }
        return nullptr;
    }

    static bool GetRawWatchFaceSize(const std::string& face, int& width, int& height) {
        if (face == "watchbyirsyad2") {
            width = WATCHBYIRSYAD2_WIDTH;
            height = WATCHBYIRSYAD2_HEIGHT;
            return true;
        }
        return false;
    }

    static uint16_t ReadRgb565(const unsigned char* data) {
        return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
    }

    static const char* WeatherCodeToText(int code) {
        if (code == 0) {
            return "SUN";
        }
        if (code == 1 || code == 2) {
            return "CLEAR";
        }
        if (code == 3) {
            return "CLOUD";
        }
        if ((code >= 45 && code <= 48)) {
            return "FOG";
        }
        if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
            return "RAIN";
        }
        if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
            return "SNOW";
        }
        if (code >= 95 && code <= 99) {
            return "STORM";
        }
        return "WX";
    }

    static std::string FormatCoordinate(int32_t value_e6) {
        char buffer[20];
        std::snprintf(buffer, sizeof(buffer), "%.6f", static_cast<double>(value_e6) / 1000000.0);
        return buffer;
    }

    std::string GetSelectedIdleFace() {
        Settings settings("idle_clock", false);
#ifdef CONFIG_BOARD_TYPE_ESP32_C6_SUPER_MINI
        auto face = settings.GetString("face", "wifi_clock");
        if (face == "wifi_clock" || face == "watchesp") {
            return face;
        }
        return "wifi_clock";
#else
        auto face = settings.GetString("face", "watchbyirsyad2");
        if (face == "wifi_clock" || face == "watchesp") {
            return face;
        }
#if DISPLAY_ROUND_SCREEN
        if (face == "waveshare") {
            return "watchbyirsyad2";
        }
#endif
        if (face == "waveshare" || GetWatchFaceData(face) != nullptr || GetRawWatchFaceData(face) != nullptr) {
            return face;
        }
        return "watchbyirsyad2";
#endif
    }

    void ApplyRoundScreenUiPadding() {
#if DISPLAY_ROUND_SCREEN
        const int icon_bar_width = width_ * 11 / 20;
        const int status_width = width_ * 2 / 3;
        const int bottom_width = width_ * 5 / 6;

        if (top_bar_ != nullptr) {
            lv_obj_set_width(top_bar_, icon_bar_width);
            lv_obj_set_style_pad_top(top_bar_, 1, 0);
            lv_obj_set_style_pad_bottom(top_bar_, 1, 0);
            lv_obj_set_style_pad_left(top_bar_, 4, 0);
            lv_obj_set_style_pad_right(top_bar_, 4, 0);
            lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, 18);
        }
        if (status_bar_ != nullptr) {
            lv_obj_set_width(status_bar_, status_width);
            lv_obj_set_style_pad_top(status_bar_, 0, 0);
            lv_obj_set_style_pad_bottom(status_bar_, 0, 0);
            lv_obj_set_style_pad_left(status_bar_, 0, 0);
            lv_obj_set_style_pad_right(status_bar_, 0, 0);
            lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 34);
        }
        if (status_label_ != nullptr) {
            lv_obj_set_width(status_label_, status_width);
            lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        }
        if (notification_label_ != nullptr) {
            lv_obj_set_width(notification_label_, status_width);
            lv_label_set_long_mode(notification_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        }
        if (bottom_bar_ != nullptr) {
            lv_obj_set_width(bottom_bar_, bottom_width);
            lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, -44);
        }
        if (chat_message_label_ != nullptr) {
            lv_obj_set_width(chat_message_label_, bottom_width - 12);
        }
        if (low_battery_popup_ != nullptr) {
            lv_obj_set_width(low_battery_popup_, bottom_width);
            lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -28);
        }
#endif
    }

    lv_obj_t* CreateTransparentOverlay(lv_obj_t* parent) {
        auto obj = lv_obj_create(parent);
        lv_obj_set_pos(obj, 0, 0);
        lv_obj_set_size(obj, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_radius(obj, 0, 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_style_pad_all(obj, 0, 0);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        return obj;
    }

    lv_obj_t* CreateRect(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t color,
                         int radius = 0, lv_opa_t opa = LV_OPA_COVER) {
        auto obj = lv_obj_create(parent);
        lv_obj_set_pos(obj, kIdleBaseX + x, kIdleBaseY + y);
        lv_obj_set_size(obj, w, h);
        lv_obj_set_style_radius(obj, radius, 0);
        lv_obj_set_style_bg_color(obj, color, 0);
        lv_obj_set_style_bg_opa(obj, opa, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_style_pad_all(obj, 0, 0);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        return obj;
    }

    lv_obj_t* CreateLabel(lv_obj_t* parent, int x, int y, const char* text, const lv_font_t* font,
                          lv_color_t color, int width = LV_SIZE_CONTENT,
                          lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
        auto label = lv_label_create(parent);
        lv_obj_set_pos(label, kIdleBaseX + x, kIdleBaseY + y);
        if (width != LV_SIZE_CONTENT) {
            lv_obj_set_width(label, width);
            lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        }
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, color, 0);
        lv_obj_set_style_text_align(label, align, 0);
        lv_label_set_text(label, text);
        return label;
    }

    lv_obj_t* CreateAbsolutePanel(lv_obj_t* parent, int x, int y, int w, int h,
                                  lv_color_t color, int radius, lv_opa_t opa) {
        auto obj = lv_obj_create(parent);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_size(obj, w, h);
        lv_obj_set_style_radius(obj, radius, 0);
        lv_obj_set_style_bg_color(obj, color, 0);
        lv_obj_set_style_bg_opa(obj, opa, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_style_pad_all(obj, 0, 0);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        return obj;
    }

    lv_obj_t* CreateAbsoluteCircle(lv_obj_t* parent, int x, int y, int size,
                                   lv_color_t color, int border_width) {
        auto obj = lv_obj_create(parent);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_size(obj, size, size);
        lv_obj_set_style_radius(obj, size / 2, 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(obj, border_width, 0);
        lv_obj_set_style_border_color(obj, color, 0);
        lv_obj_set_style_pad_all(obj, 0, 0);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        return obj;
    }

    lv_obj_t* CreateAbsoluteLabel(lv_obj_t* parent, int x, int y, int width, const char* text,
                                  const lv_font_t* font, lv_color_t color,
                                  lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
        auto label = lv_label_create(parent);
        lv_obj_set_pos(label, x, y);
        lv_obj_set_width(label, width);
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_font(label, font, 0);
        lv_obj_set_style_text_color(label, color, 0);
        lv_obj_set_style_text_align(label, align, 0);
        lv_label_set_text(label, text);
        return label;
    }

    WeatherSnapshot GetWeatherSnapshot() {
        std::lock_guard<std::mutex> lock(weather_mutex_);
        WeatherSnapshot snapshot;
        snapshot.temperature = weather_temperature_text_;
        snapshot.condition = weather_condition_text_;
        snapshot.location = weather_location_text_;
        snapshot.valid = weather_last_success_ms_ > 0;
        return snapshot;
    }

    void MarkWeatherFetchDone(bool success) {
        std::lock_guard<std::mutex> lock(weather_mutex_);
        weather_fetch_in_progress_ = false;
        if (success) {
            weather_last_success_ms_ = esp_timer_get_time() / 1000;
        }
    }

    void RequestWeatherUpdateIfNeeded() {
        // ESP32-C6 Super Mini has very limited SRAM and no PSRAM. HTTPS weather
        // refreshes can temporarily consume enough heap to destabilize audio.
        return;

        const int64_t now_ms = esp_timer_get_time() / 1000;
        {
            std::lock_guard<std::mutex> lock(weather_mutex_);
            const int64_t retry_interval = weather_last_success_ms_ > 0 ?
                                           kWeatherRefreshIntervalMs : kWeatherRetryIntervalMs;
            if (weather_fetch_in_progress_ ||
                    (weather_last_attempt_ms_ > 0 && now_ms - weather_last_attempt_ms_ < retry_interval)) {
                return;
            }
            weather_fetch_in_progress_ = true;
            weather_last_attempt_ms_ = now_ms;
        }

        auto ret = xTaskCreate([](void* arg) {
            auto* self = static_cast<SuperminiLcdDisplay*>(arg);
            self->FetchWeatherTask();
            vTaskDelete(nullptr);
        }, "weather_fetch", 4096, this, 1, nullptr);
        if (ret != pdPASS) {
            MarkWeatherFetchDone(false);
        }
    }

    void FetchWeatherTask() {
        if (!WifiManager::GetInstance().IsConnected()) {
            MarkWeatherFetchDone(false);
            return;
        }

        Settings settings("weather", false);
        const auto location = settings.GetString("location", "Jakarta");
        const int32_t latitude_e6 = settings.GetInt("latitude_e6", kDefaultWeatherLatitudeE6);
        const int32_t longitude_e6 = settings.GetInt("longitude_e6", kDefaultWeatherLongitudeE6);

        const std::string url = "https://api.open-meteo.com/v1/forecast?latitude=" +
            FormatCoordinate(latitude_e6) + "&longitude=" + FormatCoordinate(longitude_e6) +
            "&current=temperature_2m,weather_code&timezone=auto&forecast_days=1";

        auto http = Board::GetInstance().GetNetwork()->CreateHttp(2);
        http->SetTimeout(8000);
        http->SetHeader("Accept", "application/json");
        http->SetHeader("User-Agent", "xiaozhi-esp32");
        if (!http->Open("GET", url)) {
            ESP_LOGW("SuperminiLcdDisplay", "Weather request failed: 0x%x", http->GetLastError());
            MarkWeatherFetchDone(false);
            return;
        }

        const int status_code = http->GetStatusCode();
        if (status_code != 200) {
            ESP_LOGW("SuperminiLcdDisplay", "Weather HTTP status: %d", status_code);
            http->Close();
            MarkWeatherFetchDone(false);
            return;
        }

        const std::string response = http->ReadAll();
        http->Close();

        cJSON* root = cJSON_Parse(response.c_str());
        if (root == nullptr) {
            MarkWeatherFetchDone(false);
            return;
        }

        bool success = false;
        cJSON* current = cJSON_GetObjectItemCaseSensitive(root, "current");
        cJSON* temperature = cJSON_IsObject(current) ?
            cJSON_GetObjectItemCaseSensitive(current, "temperature_2m") : nullptr;
        cJSON* weather_code = cJSON_IsObject(current) ?
            cJSON_GetObjectItemCaseSensitive(current, "weather_code") : nullptr;
        if (cJSON_IsNumber(temperature)) {
            char temperature_buffer[12];
            std::snprintf(temperature_buffer, sizeof(temperature_buffer), "%.0fC", temperature->valuedouble);
            const int code = cJSON_IsNumber(weather_code) ? weather_code->valueint : -1;

            std::lock_guard<std::mutex> lock(weather_mutex_);
            weather_temperature_text_ = temperature_buffer;
            weather_condition_text_ = WeatherCodeToText(code);
            weather_location_text_ = location;
            success = true;
        }
        cJSON_Delete(root);

        MarkWeatherFetchDone(success);
    }

    void FillIdleCanvasLocked(uint16_t color) {
        if (idle_background_buffer_ == nullptr || idle_background_stride_ == 0) {
            return;
        }

        for (int y = 0; y < kIdleCanvasHeight; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   y * idle_background_stride_);
            for (int x = 0; x < kIdleCanvasWidth; ++x) {
                row[x] = color;
            }
        }
    }

    void DrawIdlePixelLocked(int x, int y, uint16_t color) {
        if (idle_background_buffer_ == nullptr || idle_background_stride_ == 0 ||
                x < 0 || x >= kIdleCanvasWidth || y < 0 || y >= kIdleCanvasHeight) {
            return;
        }
        auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                               y * idle_background_stride_);
        row[x] = color;
    }

    void DrawIdleLineLocked(int x0, int y0, int x1, int y1, uint16_t color) {
        const int dx = x1 > x0 ? x1 - x0 : x0 - x1;
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
        const int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true) {
            DrawIdlePixelLocked(x0, y0, color);
            if (x0 == x1 && y0 == y1) {
                break;
            }
            const int e2 = err * 2;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }

    void DrawIdleFilledCircleLocked(int center_x, int center_y, int radius, uint16_t color) {
        const int radius_squared = radius * radius;
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if ((x * x) + (y * y) <= radius_squared) {
                    DrawIdlePixelLocked(center_x + x, center_y + y, color);
                }
            }
        }
    }

    static int TriangleEdge(int px, int py, int ax, int ay, int bx, int by) {
        return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
    }

    void DrawIdleFilledTriangleLocked(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
        const int min_x = std::max(0, std::min({x0, x1, x2}));
        const int max_x = std::min(kIdleCanvasWidth - 1, std::max({x0, x1, x2}));
        const int min_y = std::max(0, std::min({y0, y1, y2}));
        const int max_y = std::min(kIdleCanvasHeight - 1, std::max({y0, y1, y2}));

        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const int d0 = TriangleEdge(x, y, x0, y0, x1, y1);
                const int d1 = TriangleEdge(x, y, x1, y1, x2, y2);
                const int d2 = TriangleEdge(x, y, x2, y2, x0, y0);
                if ((d0 >= 0 && d1 >= 0 && d2 >= 0) || (d0 <= 0 && d1 <= 0 && d2 <= 0)) {
                    DrawIdlePixelLocked(x, y, color);
                }
            }
        }
    }

    void ComputeWatchEspPoint(int degrees, int radius, int& x, int& y) const {
        const float radians = degrees * kPi / 180.0f;
        x = kWatchCenterX + static_cast<int>(std::round(std::cos(radians) * radius));
        y = kWatchCenterX + static_cast<int>(std::round(std::sin(radians) * radius));
    }

    void RenderWaveIdleFaceLocked() {
        if (idle_background_buffer_ == nullptr || idle_background_stride_ == 0) {
            return;
        }

        for (int y = 0; y < kIdleCanvasHeight; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   y * idle_background_stride_);
            for (int x = 0; x < kIdleCanvasWidth; ++x) {
                row[x] = 0xffff;
            }
        }

        for (int y = 0; y < kWaveBgSize; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   (kIdleBaseY + y) * idle_background_stride_);
            for (int x = 0; x < kWaveBgSize; ++x) {
                const int bit_index = y * kWaveBgSize + x;
                const bool black = (backImage[bit_index / 8] & (0x80 >> (bit_index & 7))) != 0;
                row[kIdleBaseX + x] = black ? 0x0000 : 0xffff;
            }
        }

        rendered_idle_face_ = "waveshare";
        if (idle_canvas_ != nullptr) {
            lv_obj_invalidate(idle_canvas_);
        }
    }

    bool RenderWatchIdleFaceLocked(const std::string& face, const unsigned char* face_data) {
        if (idle_background_buffer_ == nullptr || idle_background_stride_ == 0 || face_data == nullptr) {
            return false;
        }

        const int width = static_cast<int>(face_data[0]) | (static_cast<int>(face_data[1]) << 8);
        const int height = static_cast<int>(face_data[2]) | (static_cast<int>(face_data[3]) << 8);
        const int bits_per_pixel = face_data[4];
        if (width != 240 || height != 240 || bits_per_pixel != 2) {
            ESP_LOGW("SuperminiLcdDisplay", "Unsupported idle clock face %s (%dx%dx%d)",
                     face.c_str(), width, height, bits_per_pixel);
            return false;
        }

        uint16_t palette[4] = {};
        const unsigned char* palette_data = face_data + 6;
        for (int i = 0; i < 4; ++i) {
            palette[i] = ReadRgb565(palette_data + (i * 2));
        }

        const unsigned char* pixels = face_data + 6 + (4 * 2);
        const int bytes_per_row = width / 4;
        for (int y = 0; y < kIdleCanvasHeight; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   y * idle_background_stride_);
            int src_y = y * height / kIdleCanvasHeight;
            const unsigned char* src = pixels + (src_y * bytes_per_row);
            for (int byte_index = 0; byte_index < bytes_per_row; ++byte_index) {
                const uint8_t value = src[byte_index];
                const int x = byte_index * 4;
                row[x + 0] = palette[(value >> 6) & 0x03];
                row[x + 1] = palette[(value >> 4) & 0x03];
                row[x + 2] = palette[(value >> 2) & 0x03];
                row[x + 3] = palette[value & 0x03];
            }
        }

        rendered_idle_face_ = face;
        if (idle_canvas_ != nullptr) {
            lv_obj_invalidate(idle_canvas_);
        }
        return true;
    }

    bool RenderRawWatchIdleFaceLocked(const std::string& face, const uint16_t* face_data) {
        if (idle_background_buffer_ == nullptr || idle_background_stride_ == 0 || face_data == nullptr) {
            return false;
        }
        int width = 0;
        int height = 0;
        if (!GetRawWatchFaceSize(face, width, height) || width != 240 || height != 240) {
            ESP_LOGW("SuperminiLcdDisplay", "Unsupported raw idle clock face %s (%dx%d)",
                     face.c_str(), width, height);
            return false;
        }

        for (int y = 0; y < kIdleCanvasHeight; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   y * idle_background_stride_);
            int src_y = y * height / kIdleCanvasHeight;
            std::memcpy(row, face_data + (src_y * width), width * sizeof(uint16_t));
        }

        rendered_idle_face_ = face;
        if (idle_canvas_ != nullptr) {
            lv_obj_invalidate(idle_canvas_);
        }
        return true;
    }

    void RenderWifiClockFaceLocked() {
        if (idle_background_buffer_ == nullptr || idle_background_stride_ == 0) {
            return;
        }

        for (int y = 0; y < kIdleCanvasHeight; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   y * idle_background_stride_);
            for (int x = 0; x < kIdleCanvasWidth; ++x) {
                row[x] = 0x0000;
            }
        }

        rendered_idle_face_ = "wifi_clock";
        if (idle_canvas_ != nullptr) {
            lv_obj_invalidate(idle_canvas_);
        }
    }

    void RenderWatchEspFaceLocked(int second) {
        if (idle_background_buffer_ == nullptr || idle_background_stride_ == 0) {
            return;
        }

        second = ((second % 60) + 60) % 60;
        const int angle = second * 6;
        const uint16_t black = 0x0000;
        const uint16_t white = 0xffff;
        const uint16_t teal = 0x00a3;
        const uint16_t orange = 0xfd20;
        const uint16_t red = 0xf800;

        FillIdleCanvasLocked(black);
        DrawIdleFilledCircleLocked(kWatchCenterX, kWatchCenterY, 124, teal);

        for (int i = 0; i < 12; ++i) {
            const int point_degrees = (i * 30 + angle) % 360;
            int inner_x = 0;
            int inner_y = 0;
            int outer_x = 0;
            int outer_y = 0;
            ComputeWatchEspPoint(point_degrees, 78, inner_x, inner_y);
            ComputeWatchEspPoint(point_degrees, 88, outer_x, outer_y);
            DrawIdleLineLocked(outer_x, outer_y, inner_x, inner_y, white);
        }

        for (int i = 0; i < 60; ++i) {
            const int point_degrees = (i * 6 + angle) % 360;
            int dot_x = 0;
            int dot_y = 0;
            ComputeWatchEspPoint(point_degrees, 88, dot_x, dot_y);
            DrawIdleFilledCircleLocked(dot_x, dot_y, 1, white);
        }

        DrawIdleFilledTriangleLocked(kWatchCenterX - 1, kWatchCenterY - 70,
                                     kWatchCenterX - 5, kWatchCenterX - 56,
                                     kWatchCenterX + 4, kWatchCenterX - 56,
                                     orange);

        int red_x = 0;
        int red_y = 0;
        ComputeWatchEspPoint((359 - angle + 360) % 360, 88, red_x, red_y);
        DrawIdleFilledCircleLocked(red_x, red_y, 6, red);

        rendered_idle_face_ = "watchesp";
        if (idle_canvas_ != nullptr) {
            lv_obj_invalidate(idle_canvas_);
        }
    }

    bool ApplyIdleFaceLocked() {
        auto face = GetSelectedIdleFace();
        const bool use_wifi_clock = face == "wifi_clock";
        const bool use_watchesp = face == "watchesp";
        const unsigned char* watch_face = GetWatchFaceData(face);
        const uint16_t* raw_watch_face = GetRawWatchFaceData(face);
        bool use_watch_face = watch_face != nullptr || raw_watch_face != nullptr;

        if (rendered_idle_face_ != face) {
            if (use_wifi_clock) {
                RenderWifiClockFaceLocked();
            } else if (use_watchesp) {
                RenderWatchEspFaceLocked(30);
            } else if (raw_watch_face != nullptr) {
                if (!RenderRawWatchIdleFaceLocked(face, raw_watch_face)) {
                    use_watch_face = false;
                    RenderWaveIdleFaceLocked();
                }
            } else if (watch_face != nullptr) {
                if (!RenderWatchIdleFaceLocked(face, watch_face)) {
                    use_watch_face = false;
                    RenderWaveIdleFaceLocked();
                }
            } else {
                RenderWaveIdleFaceLocked();
            }
        }

        if (idle_wave_overlay_ != nullptr) {
            if (use_watch_face || use_wifi_clock || use_watchesp) {
                lv_obj_add_flag(idle_wave_overlay_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_remove_flag(idle_wave_overlay_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (idle_watch_overlay_ != nullptr) {
            if (use_watch_face) {
                lv_obj_remove_flag(idle_watch_overlay_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(idle_watch_overlay_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (idle_wifi_clock_overlay_ != nullptr) {
            if (use_wifi_clock) {
                lv_obj_remove_flag(idle_wifi_clock_overlay_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(idle_wifi_clock_overlay_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (idle_watchesp_overlay_ != nullptr) {
            if (use_watchesp) {
                lv_obj_remove_flag(idle_watchesp_overlay_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(idle_watchesp_overlay_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        return use_watch_face || use_wifi_clock || use_watchesp;
    }

    void CreateIdleBitmapCanvas() {
#ifdef CONFIG_BOARD_TYPE_ESP32_C6_SUPER_MINI
        ESP_LOGI("SuperminiLcdDisplay", "Skipping idle bitmap canvas on ESP32-C6 to preserve SRAM");
        return;
#endif
        idle_background_stride_ = lv_draw_buf_width_to_stride(kIdleCanvasWidth, LV_COLOR_FORMAT_RGB565);
        const size_t buffer_size = idle_background_stride_ * kIdleCanvasHeight;
        idle_background_buffer_ = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (idle_background_buffer_ == nullptr) {
            idle_background_buffer_ = heap_caps_malloc(buffer_size, MALLOC_CAP_8BIT);
        }
        if (idle_background_buffer_ == nullptr) {
            ESP_LOGW("SuperminiLcdDisplay", "Unable to allocate idle clock background buffer");
            return;
        }

        std::memset(idle_background_buffer_, 0xff, buffer_size);
        idle_canvas_ = lv_canvas_create(idle_clock_layer_);
        lv_canvas_set_buffer(idle_canvas_, idle_background_buffer_, kIdleCanvasWidth, kIdleCanvasHeight,
                             LV_COLOR_FORMAT_RGB565);
        lv_obj_set_pos(idle_canvas_, 0, 0);
        lv_obj_clear_flag(idle_canvas_, LV_OBJ_FLAG_SCROLLABLE);
        RenderWaveIdleFaceLocked();
    }

    lv_obj_t* CreateAnalogLine(lv_obj_t* parent, lv_point_precise_t points[2], int width, lv_color_t color) {
        auto line = lv_line_create(parent);
        lv_obj_set_pos(line, 0, 0);
        lv_obj_set_size(line, kIdleCanvasWidth, kIdleCanvasHeight);
        lv_line_set_points_mutable(line, points, 2);
        lv_obj_set_style_line_width(line, width, 0);
        lv_obj_set_style_line_color(line, color, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        return line;
    }

    void MoveAnalogHandsToForegroundLocked() {
        lv_obj_t* objects[] = {
            analog_hour_line_,
            analog_minute_line_,
            analog_second_line_,
            analog_center_dot_,
        };
        for (auto* obj : objects) {
            if (obj != nullptr) {
                lv_obj_move_foreground(obj);
            }
        }
    }

    void SetAnalogHandsVisibleLocked(bool visible) {
        lv_obj_t* objects[] = {
            analog_hour_line_,
            analog_minute_line_,
            analog_second_line_,
            analog_center_dot_,
        };
        for (auto* obj : objects) {
            if (obj == nullptr) {
                continue;
            }
            if (visible) {
                lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    void CreateAnalogOverlay() {
        idle_watch_overlay_ = CreateTransparentOverlay(idle_clock_layer_);
        analog_hour_line_ = CreateAnalogLine(idle_watch_overlay_, analog_hour_points_, 7, lv_color_hex(0x202020));
        analog_minute_line_ = CreateAnalogLine(idle_watch_overlay_, analog_minute_points_, 5, lv_color_hex(0x202020));
        analog_second_line_ = CreateAnalogLine(idle_watch_overlay_, analog_second_points_, 2, lv_color_hex(0xd23b3b));

        analog_center_dot_ = lv_obj_create(idle_watch_overlay_);
        lv_obj_set_size(analog_center_dot_, 12, 12);
        lv_obj_set_pos(analog_center_dot_, kWatchCenterX - 6, kWatchCenterY - 6);
        lv_obj_set_style_radius(analog_center_dot_, 6, 0);
        lv_obj_set_style_bg_color(analog_center_dot_, lv_color_hex(0x202020), 0);
        lv_obj_set_style_bg_opa(analog_center_dot_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(analog_center_dot_, 2, 0);
        lv_obj_set_style_border_color(analog_center_dot_, lv_color_white(), 0);
        lv_obj_clear_flag(analog_center_dot_, LV_OBJ_FLAG_SCROLLABLE);

        // watchbyirsyad1 removed to save flash space

        watchbyirsyad2_weather_label_ = CreateAbsoluteLabel(idle_watch_overlay_, 101, 76, 38, "--C",
                                                            &lv_font_montserrat_14, lv_color_white());
        watchbyirsyad2_weekday_label_ = CreateAbsoluteLabel(idle_watch_overlay_, 82, 128, 76, "---",
                                                            &lv_font_montserrat_14, lv_color_white());
        watchbyirsyad2_date_label_ = CreateAbsoluteLabel(idle_watch_overlay_, 78, 143, 84, "-- --- ----",
                                                         &lv_font_montserrat_14, lv_color_white());
        SetWatchByIrsyad2InfoVisibleLocked(false);

        lv_obj_add_flag(idle_watch_overlay_, LV_OBJ_FLAG_HIDDEN);
    }

    void CreateWifiClockOverlay() {
        idle_wifi_clock_overlay_ = CreateTransparentOverlay(idle_clock_layer_);

        const auto pink = lv_color_hex(0xff3bd5);
        const auto cyan = lv_color_hex(0x00f5ff);
        const auto yellow = lv_color_hex(0xffd447);
        const auto dark = lv_color_hex(0x18002a);

        CreateAbsoluteCircle(idle_wifi_clock_overlay_, 3, 3, 234, pink, 2);
        CreateAbsoluteCircle(idle_wifi_clock_overlay_, 10, 10, 220, dark, 2);
        CreateAbsoluteCircle(idle_wifi_clock_overlay_, 17, 17, 206, lv_color_hex(0x080812), 1);

        wifi_clock_time_label_ = CreateAbsoluteLabel(idle_wifi_clock_overlay_, 28, 92, 184, "--:--:--",
                                                     &lv_font_montserrat_28, cyan);
        wifi_clock_date_label_ = CreateAbsoluteLabel(idle_wifi_clock_overlay_, 54, 142, 132, "--- -- ----",
                                                     &lv_font_montserrat_14, pink);
        wifi_clock_status_label_ = CreateAbsoluteLabel(idle_wifi_clock_overlay_, 88, 203, 64, "NO WIFI",
                                                       &lv_font_montserrat_14, yellow);

        for (int i = 0; i < 4; ++i) {
            const int bar_height = 6 + (i * 4);
            wifi_clock_bars_[i] = CreateAbsolutePanel(idle_wifi_clock_overlay_, 103 + (i * 9),
                                                      190 - bar_height, 5, bar_height,
                                                      dark, 1, LV_OPA_COVER);
        }

        lv_obj_add_flag(idle_wifi_clock_overlay_, LV_OBJ_FLAG_HIDDEN);
    }

    void CreateWatchEspOverlay() {
        idle_watchesp_overlay_ = CreateTransparentOverlay(idle_clock_layer_);

        const auto white = lv_color_white();
        const auto orange = lv_color_hex(0xffa000);
        const auto blue = lv_color_hex(0x35d7);
        const auto box = lv_color_hex(0x5acb);

        static const char* ring_texts[] = {
            "45", "40", "35", "30", "25", "20", "15", "10", "05", "0", "55", "50",
        };
        for (int i = 0; i < 12; ++i) {
            watchesp_ring_labels_[i] = CreateAbsoluteLabel(idle_watchesp_overlay_, 0, 0, 28,
                                                           ring_texts[i], &lv_font_montserrat_14, white);
        }

        watchesp_weekday_label_ = CreateAbsoluteLabel(idle_watchesp_overlay_, 50, 112, 140, "SUNDAY",
                                                      &lv_font_montserrat_14, white);
        watchesp_second_label_ = CreateAbsoluteLabel(idle_watchesp_overlay_, 96, 74, 48, "30",
                                                     &lv_font_montserrat_24, white);
        watchesp_hour_minute_label_ = CreateAbsoluteLabel(idle_watchesp_overlay_, 58, 132, 124, "10:10",
                                                          &lv_font_montserrat_28, white);

        CreateAbsolutePanel(idle_watchesp_overlay_, 70, 86, 12, 20, box, 0, LV_OPA_COVER);
        CreateAbsolutePanel(idle_watchesp_overlay_, 84, 86, 12, 20, box, 0, LV_OPA_COVER);
        CreateAbsolutePanel(idle_watchesp_overlay_, 150, 86, 12, 20, box, 0, LV_OPA_COVER);
        CreateAbsolutePanel(idle_watchesp_overlay_, 164, 86, 12, 20, box, 0, LV_OPA_COVER);

        CreateAbsoluteLabel(idle_watchesp_overlay_, 57, 75, 52, "MONTH", &lv_font_montserrat_14, blue);
        CreateAbsoluteLabel(idle_watchesp_overlay_, 145, 75, 36, "DAY", &lv_font_montserrat_14, blue);
        CreateAbsoluteLabel(idle_watchesp_overlay_, 98, 103, 44, "***", &lv_font_montserrat_14, orange);
        CreateAbsoluteLabel(idle_watchesp_overlay_, 51, 174, 138, "VOLOS PROJECTS",
                            &lv_font_montserrat_14, orange);

        watchesp_month_digits_[0] = CreateAbsoluteLabel(idle_watchesp_overlay_, 70, 89, 12, "0",
                                                        &lv_font_montserrat_14, white);
        watchesp_month_digits_[1] = CreateAbsoluteLabel(idle_watchesp_overlay_, 84, 89, 12, "1",
                                                        &lv_font_montserrat_14, white);
        watchesp_day_digits_[0] = CreateAbsoluteLabel(idle_watchesp_overlay_, 150, 89, 12, "0",
                                                      &lv_font_montserrat_14, white);
        watchesp_day_digits_[1] = CreateAbsoluteLabel(idle_watchesp_overlay_, 164, 89, 12, "1",
                                                      &lv_font_montserrat_14, white);

        lv_obj_add_flag(idle_watchesp_overlay_, LV_OBJ_FLAG_HIDDEN);
    }

    void SetWatchByIrsyad1InfoVisibleLocked(bool visible) {
        // Removed
    }

    void SetWatchByIrsyad2InfoVisibleLocked(bool visible) {
        lv_obj_t* objects[] = {
            watchbyirsyad2_weather_label_,
            watchbyirsyad2_weekday_label_,
            watchbyirsyad2_date_label_,
        };
        for (auto* obj : objects) {
            if (obj == nullptr) {
                continue;
            }
            if (visible) {
                lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    void EraseWatchByIrsyad1TextRectLocked(int x, int y, int w, int h, int sample_y) {
        // Removed
    }

    void EraseWatchByIrsyad2TextRectLocked(int x, int y, int w, int h, int sample_y) {
        if (idle_background_buffer_ == nullptr || idle_background_stride_ == 0) {
            return;
        }

        sample_y = std::clamp(sample_y, 0, kIdleCanvasWidth - 1);
        for (int yy = y; yy < y + h; ++yy) {
            if (yy < 0 || yy >= kIdleCanvasHeight) {
                continue;
            }
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   yy * idle_background_stride_);
            for (int xx = x; xx < x + w; ++xx) {
                if (xx >= 0 && xx < kIdleCanvasWidth) {
                    row[xx] = epd_bitmap_watchbyirsyad2[sample_y * kIdleCanvasWidth + xx];
                }
            }
        }
    }

    std::string FormatWatchByIrsyad1Temperature(const WeatherSnapshot& weather) {
        if (!weather.valid || weather.temperature.empty()) {
            return "--";
        }
        std::string text = weather.temperature;
        if (!text.empty() && text.back() == 'C') {
            text.pop_back();
            text += "\xC2\xB0";
        }
        return text;
    }

    void UpdateWifiClockInfoLocked(const char* time_text, const char* date_text) {
        if (rendered_idle_face_ != "wifi_clock") {
            return;
        }

        if (wifi_clock_time_label_ != nullptr) {
            lv_label_set_text(wifi_clock_time_label_, time_text);
        }
        if (wifi_clock_date_label_ != nullptr) {
            lv_label_set_text(wifi_clock_date_label_, date_text);
        }

        const bool connected = WifiManager::GetInstance().IsConnected();
        const int rssi = connected ? WifiManager::GetInstance().GetRssi() : -100;
        int active_bars = 0;
        if (connected) {
            if (rssi >= -55) {
                active_bars = 4;
            } else if (rssi >= -67) {
                active_bars = 3;
            } else if (rssi >= -80) {
                active_bars = 2;
            } else {
                active_bars = 1;
            }
        }

        const auto yellow = lv_color_hex(0xffd447);
        const auto dark = lv_color_hex(0x18002a);
        for (int i = 0; i < 4; ++i) {
            if (wifi_clock_bars_[i] != nullptr) {
                lv_obj_set_style_bg_color(wifi_clock_bars_[i], i < active_bars ? yellow : dark, 0);
            }
        }
        if (wifi_clock_status_label_ != nullptr) {
            lv_label_set_text(wifi_clock_status_label_, connected ? "STABLE" : "NO WIFI");
        }
    }

    void UpdateWatchEspInfoLocked(const struct tm& time_info) {
        if (rendered_idle_face_ != "watchesp") {
            return;
        }

        int hour = 10;
        int minute = 10;
        int second = 30;
        int month = 1;
        int day = 1;
        int weekday_index = 0;
        if (time_info.tm_year >= 2025 - 1900) {
            hour = time_info.tm_hour;
            minute = time_info.tm_min;
            second = time_info.tm_sec;
            month = time_info.tm_mon + 1;
            day = time_info.tm_mday;
            weekday_index = std::clamp(time_info.tm_wday, 0, 6);
        }

        hour = std::clamp(hour, 0, 23);
        minute = std::clamp(minute, 0, 59);
        second = std::clamp(second, 0, 59);
        month = std::clamp(month, 1, 12);
        day = std::clamp(day, 1, 31);
        RenderWatchEspFaceLocked(second);

        auto two_digits = [](int value, char* buffer) {
            buffer[0] = static_cast<char>('0' + (value / 10));
            buffer[1] = static_cast<char>('0' + (value % 10));
            buffer[2] = '\0';
        };

        char hour_digits[3];
        char minute_digits[3];
        char seconds[3];
        char month_digits[3];
        char day_digits[3];
        two_digits(hour, hour_digits);
        two_digits(minute, minute_digits);
        two_digits(second, seconds);
        two_digits(month, month_digits);
        two_digits(day, day_digits);
        char hour_minute[6] = {
            hour_digits[0], hour_digits[1], ':', minute_digits[0], minute_digits[1], '\0',
        };

        if (watchesp_hour_minute_label_ != nullptr) {
            lv_label_set_text(watchesp_hour_minute_label_, hour_minute);
        }
        if (watchesp_second_label_ != nullptr) {
            lv_label_set_text(watchesp_second_label_, seconds);
        }

        static const char* weekdays[] = {
            "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY",
        };
        if (watchesp_weekday_label_ != nullptr) {
            const int weekday_offset = static_cast<int>(std::round(std::sin(second * 6.0f * kPi / 180.0f) * 20.0f));
            lv_obj_set_pos(watchesp_weekday_label_, 50 + weekday_offset, 112);
            lv_label_set_text(watchesp_weekday_label_, weekdays[weekday_index]);
        }

        for (int i = 0; i < 2; ++i) {
            char value[2] = {month_digits[i], '\0'};
            if (watchesp_month_digits_[i] != nullptr) {
                lv_label_set_text(watchesp_month_digits_[i], value);
            }
            value[0] = day_digits[i];
            if (watchesp_day_digits_[i] != nullptr) {
                lv_label_set_text(watchesp_day_digits_[i], value);
            }
        }

        static const char* ring_texts[] = {
            "45", "40", "35", "30", "25", "20", "15", "10", "05", "0", "55", "50",
        };
        const int angle = second * 6;
        for (int i = 0; i < 12; ++i) {
            if (watchesp_ring_labels_[i] == nullptr) {
                continue;
            }
            int x = 0;
            int y = 0;
            ComputeWatchEspPoint((i * 30 + angle) % 360, 104, x, y);
            lv_obj_set_pos(watchesp_ring_labels_[i], x - 14, y - 8);
            lv_label_set_text(watchesp_ring_labels_[i], ring_texts[i]);
        }
    }

    void UpdateWatchByIrsyad1InfoLocked(const char* hour_text, const char* minute_text, const char* day_text,
                                        const char* date_text, const char* month_text, const char* year_text,
                                        const WeatherSnapshot& weather) {
        // Removed
    }

    void UpdateWatchByIrsyad2InfoLocked(const char* date_text, const char* weekday_text,
                                        const WeatherSnapshot& weather) {
        const bool visible = rendered_idle_face_ == "watchbyirsyad2";
        SetWatchByIrsyad2InfoVisibleLocked(visible);
        if (!visible) {
            return;
        }

        int mapped_y = 75 * kIdleCanvasHeight / 240;
        int sample_y = 92 * kIdleCanvasHeight / 240;
        EraseWatchByIrsyad2TextRectLocked(102, mapped_y, 36, 17, sample_y);
        if (idle_canvas_ != nullptr) {
            lv_obj_invalidate(idle_canvas_);
        }

        if (watchbyirsyad2_date_label_ != nullptr) {
            lv_label_set_text(watchbyirsyad2_date_label_, date_text);
        }
        if (watchbyirsyad2_weekday_label_ != nullptr) {
            lv_label_set_text(watchbyirsyad2_weekday_label_, weekday_text);
        }
        if (watchbyirsyad2_weather_label_ != nullptr) {
            lv_label_set_text(watchbyirsyad2_weather_label_, weather.valid ? weather.temperature.c_str() : "--C");
        }
        MoveAnalogHandsToForegroundLocked();
    }

    void SetAnalogHandPoints(lv_point_precise_t points[2], float degrees, int front_length, int back_length) {
        const float radians = (degrees - 90.0f) * kPi / 180.0f;
        const float dx = std::cos(radians);
        const float dy = std::sin(radians);
        points[0].x = static_cast<int32_t>(kWatchCenterX - (dx * back_length));
        points[0].y = static_cast<int32_t>(kWatchCenterY - (dy * back_length));
        points[1].x = static_cast<int32_t>(kWatchCenterX + (dx * front_length));
        points[1].y = static_cast<int32_t>(kWatchCenterY + (dy * front_length));
    }

    void UpdateAnalogHandsLocked(const struct tm& time_info) {
        const bool visible = rendered_idle_face_ != "watchbyirsyad1" &&
                             rendered_idle_face_ != "wifi_clock" &&
                             rendered_idle_face_ != "watchesp";
        SetAnalogHandsVisibleLocked(visible);
        if (!visible) {
            return;
        }

        int hour = 10;
        int minute = 10;
        int second = 30;
        if (time_info.tm_year >= 2025 - 1900) {
            hour = time_info.tm_hour;
            minute = time_info.tm_min;
            second = time_info.tm_sec;
        }

        const float second_degrees = second * 6.0f;
        const float minute_degrees = (minute * 6.0f) + (second * 0.1f);
        const float hour_degrees = ((hour % 12) * 30.0f) + (minute * 0.5f);

        SetAnalogHandPoints(analog_hour_points_, hour_degrees, 58, 10);
        SetAnalogHandPoints(analog_minute_points_, minute_degrees, 84, 12);
        SetAnalogHandPoints(analog_second_points_, second_degrees, 92, 18);

        if (analog_hour_line_ != nullptr) {
            lv_obj_invalidate(analog_hour_line_);
        }
        if (analog_minute_line_ != nullptr) {
            lv_obj_invalidate(analog_minute_line_);
        }
        if (analog_second_line_ != nullptr) {
            lv_obj_invalidate(analog_second_line_);
        }
    }

    void CreateIdleClockLayer() {
        if (idle_clock_layer_ != nullptr) {
            return;
        }

        auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
        auto text_font = lvgl_theme->text_font()->font();
        auto screen = lv_screen_active();

        idle_clock_layer_ = lv_obj_create(screen);
        lv_obj_set_size(idle_clock_layer_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_radius(idle_clock_layer_, 0, 0);
        lv_obj_set_style_bg_color(idle_clock_layer_, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(idle_clock_layer_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(idle_clock_layer_, 0, 0);
        lv_obj_set_style_pad_all(idle_clock_layer_, 0, 0);
        lv_obj_clear_flag(idle_clock_layer_, LV_OBJ_FLAG_SCROLLABLE);

        CreateIdleBitmapCanvas();

        idle_wave_overlay_ = CreateTransparentOverlay(idle_clock_layer_);

        CreateRect(idle_wave_overlay_, 60, 137, 124, 5, lv_color_black());
        CreateRect(idle_wave_overlay_, 120, 82, 60, 2, lv_color_black());
        CreateRect(idle_wave_overlay_, 10, 42, 3, 129, lv_color_black());

        auto battery = lv_obj_create(idle_wave_overlay_);
        lv_obj_set_pos(battery, kIdleBaseX + 150, kIdleBaseY + 8);
        lv_obj_set_size(battery, 40, 16);
        lv_obj_set_style_bg_opa(battery, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(battery, 2, 0);
        lv_obj_set_style_border_color(battery, lv_color_black(), 0);
        lv_obj_set_style_radius(battery, 0, 0);
        lv_obj_set_style_pad_all(battery, 0, 0);
        lv_obj_clear_flag(battery, LV_OBJ_FLAG_SCROLLABLE);
        CreateRect(idle_wave_overlay_, 190, 12, 3, 7, lv_color_black());
        for (int i = 0; i < 5; ++i) {
            idle_battery_segments_[i] = CreateRect(idle_wave_overlay_, 154 + (i * 7), 12, 4, 8, lv_color_black());
        }

        CreateRect(idle_wave_overlay_, 20, 40, 95, 45, lv_color_black(), 5);
        CreateRect(idle_wave_overlay_, 152, 94, 30, 22, lv_color_black(), 4);

        idle_time_label_ = CreateLabel(idle_wave_overlay_, 17, 91, "--:--", &font_puhui_basic_30_4,
                                       lv_color_black(), 118, LV_TEXT_ALIGN_CENTER);
        CreateLabel(idle_wave_overlay_, 26, 36, "DATE", text_font, lv_color_white());
        idle_date_label_ = CreateLabel(idle_wave_overlay_, 26, 58, "--/--/--", text_font, lv_color_white());
        CreateLabel(idle_wave_overlay_, 120, 42, "TIME", text_font, lv_color_black());
        CreateLabel(idle_wave_overlay_, 120, 65, "XIAOZHI", text_font, lv_color_black());
        idle_weekday_label_ = CreateLabel(idle_wave_overlay_, 152, 94, "--", text_font, lv_color_white(),
                                          30, LV_TEXT_ALIGN_CENTER);
        CreateLabel(idle_wave_overlay_, 60, 141, "TEMP", text_font, lv_color_black());
        idle_temperature_label_ = CreateLabel(idle_wave_overlay_, 60, 163, "--C", text_font, lv_color_black());
        CreateLabel(idle_wave_overlay_, 135, 141, "WAKE", text_font, lv_color_black());
        idle_wake_label_ = CreateLabel(idle_wave_overlay_, 127, 163, "HI JASON", text_font, lv_color_black());

        CreateAnalogOverlay();
        CreateWifiClockOverlay();
        CreateWatchEspOverlay();

        lv_obj_add_flag(idle_clock_layer_, LV_OBJ_FLAG_HIDDEN);
    }

    void ShowIdleClockLocked(bool show) {
        if (idle_clock_layer_ == nullptr) {
            return;
        }
        idle_clock_visible_ = show;
        if (show) {
            lv_obj_remove_flag(idle_clock_layer_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(idle_clock_layer_);
            if (gif_controller_) {
                gif_controller_->Stop();
            }
            if (high_temp_popup_ != nullptr) {
                lv_obj_move_foreground(high_temp_popup_);
            }
        } else {
            lv_obj_add_flag(idle_clock_layer_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    void UpdateIdleClockLocked(const char* status) {
        if (idle_time_label_ == nullptr) {
            return;
        }

        RequestWeatherUpdateIfNeeded();
        WeatherSnapshot weather = GetWeatherSnapshot();

        char time_buffer[8] = "--:--";
        char time_seconds_buffer[9] = "--:--:--";
        char date_buffer[16] = "--/--/----";
        char wifi_date_buffer[16] = "--- -- ----";
        char watch_date_buffer[20] = "-- --- ----";
        char watch1_hour_buffer[3] = "--";
        char watch1_minute_buffer[3] = "--";
        char watch1_day_buffer[4] = "---";
        char watch1_date_buffer[3] = "--";
        char watch1_month_buffer[4] = "---";
        char watch1_year_buffer[12] = "----";
        const char* weekdays[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
        const char* long_weekdays[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
        const char* watch1_weekdays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        const char* watch1_months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                       "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        const char* weekday = "--";
        const char* long_weekday = "---";

        time_t now = time(nullptr);
        struct tm time_info = {};
        struct tm* local = localtime(&now);
        if (local != nullptr) {
            time_info = *local;
        }

        if (IsTimeStatus(status)) {
            std::snprintf(time_buffer, sizeof(time_buffer), "%s", status);
        } else if (time_info.tm_year >= 2025 - 1900) {
            std::strftime(time_buffer, sizeof(time_buffer), "%H:%M", &time_info);
        }
        if (time_info.tm_year >= 2025 - 1900) {
            std::strftime(time_seconds_buffer, sizeof(time_seconds_buffer), "%H:%M:%S", &time_info);
        } else if (IsTimeStatus(time_buffer)) {
            time_seconds_buffer[0] = time_buffer[0];
            time_seconds_buffer[1] = time_buffer[1];
            time_seconds_buffer[2] = ':';
            time_seconds_buffer[3] = time_buffer[3];
            time_seconds_buffer[4] = time_buffer[4];
            time_seconds_buffer[5] = ':';
            time_seconds_buffer[6] = '-';
            time_seconds_buffer[7] = '-';
            time_seconds_buffer[8] = '\0';
        }
        if (IsTimeStatus(time_buffer)) {
            std::snprintf(watch1_hour_buffer, sizeof(watch1_hour_buffer), "%c%c", time_buffer[0], time_buffer[1]);
            std::snprintf(watch1_minute_buffer, sizeof(watch1_minute_buffer), "%c%c", time_buffer[3], time_buffer[4]);
        }

        if (time_info.tm_year >= 2025 - 1900) {
            std::strftime(date_buffer, sizeof(date_buffer), "%d/%m/%Y", &time_info);
            std::strftime(wifi_date_buffer, sizeof(wifi_date_buffer), "%b %d %Y", &time_info);
            std::strftime(watch_date_buffer, sizeof(watch_date_buffer), "%d %b %Y", &time_info);
            for (char* p = wifi_date_buffer; *p != '\0'; ++p) {
                if (*p >= 'a' && *p <= 'z') {
                    *p = static_cast<char>(*p - ('a' - 'A'));
                }
            }
            std::snprintf(watch1_date_buffer, sizeof(watch1_date_buffer), "%02d", time_info.tm_mday);
            std::snprintf(watch1_year_buffer, sizeof(watch1_year_buffer), "%04d", time_info.tm_year + 1900);
            for (char* p = watch_date_buffer; *p != '\0'; ++p) {
                if (*p >= 'a' && *p <= 'z') {
                    *p = static_cast<char>(*p - ('a' - 'A'));
                }
            }
            if (time_info.tm_wday >= 0 && time_info.tm_wday < 7) {
                weekday = weekdays[time_info.tm_wday];
                long_weekday = long_weekdays[time_info.tm_wday];
                std::snprintf(watch1_day_buffer, sizeof(watch1_day_buffer), "%s", watch1_weekdays[time_info.tm_wday]);
            }
            if (time_info.tm_mon >= 0 && time_info.tm_mon < 12) {
                std::snprintf(watch1_month_buffer, sizeof(watch1_month_buffer), "%s", watch1_months[time_info.tm_mon]);
            }
        }

        ApplyIdleFaceLocked();
        UpdateAnalogHandsLocked(time_info);
        UpdateWifiClockInfoLocked(time_seconds_buffer, wifi_date_buffer);
        UpdateWatchEspInfoLocked(time_info);
        UpdateWatchByIrsyad1InfoLocked(watch1_hour_buffer, watch1_minute_buffer, watch1_day_buffer,
                                       watch1_date_buffer, watch1_month_buffer, watch1_year_buffer, weather);
        UpdateWatchByIrsyad2InfoLocked(watch_date_buffer, long_weekday, weather);

        lv_label_set_text(idle_time_label_, time_buffer);
        lv_label_set_text(idle_date_label_, date_buffer);
        lv_label_set_text(idle_weekday_label_, weekday);

        int battery_level = 0;
        bool charging = false;
        bool discharging = false;
        int active_segments = 0;
        if (Board::GetInstance().GetBatteryLevel(battery_level, charging, discharging)) {
            active_segments = (battery_level + 19) / 20;
            if (active_segments < 0) active_segments = 0;
            if (active_segments > 5) active_segments = 5;
        }
        for (int i = 0; i < 5; ++i) {
            if (idle_battery_segments_[i] != nullptr) {
                if (i < active_segments || charging) {
                    lv_obj_remove_flag(idle_battery_segments_[i], LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(idle_battery_segments_[i], LV_OBJ_FLAG_HIDDEN);
                }
            }
        }

        float temperature = 0.0f;
        char temperature_buffer[12] = "--C";
        if (weather.valid) {
            std::snprintf(temperature_buffer, sizeof(temperature_buffer), "%s", weather.temperature.c_str());
        } else if (Board::GetInstance().GetTemperature(temperature)) {
            std::snprintf(temperature_buffer, sizeof(temperature_buffer), "%.0fC", temperature);
        }
        lv_label_set_text(idle_temperature_label_, temperature_buffer);
        lv_label_set_text(idle_wake_label_, "HI JASON");
        last_status_update_time_ = std::chrono::system_clock::now();
    }

public:
    using SpiLcdDisplay::SpiLcdDisplay;

    ~SuperminiLcdDisplay() override {
        if (idle_background_buffer_ != nullptr) {
            heap_caps_free(idle_background_buffer_);
            idle_background_buffer_ = nullptr;
        }
    }

    void NextIdleTheme() {
#ifdef CONFIG_BOARD_TYPE_ESP32_C6_SUPER_MINI
        ESP_LOGI("SuperminiLcdDisplay", "Idle smartwatch themes disabled on ESP32-C6");
        return;
#else
        const char* themes[] = {
            "watchbyirsyad2",
            "watchesp",
            "wifi_clock",
            "smartq02",
            "smartq04",
            "smartq07",
            "smartq08",
            "smartq09"
        };
        const int num_themes = sizeof(themes) / sizeof(themes[0]);
        
        std::string current = GetSelectedIdleFace();
        int next_idx = 0;
        for (int i = 0; i < num_themes; ++i) {
            if (current == themes[i]) {
                next_idx = (i + 1) % num_themes;
                break;
            }
        }
        
        Settings settings("idle_clock", true);
        settings.SetString("face", themes[next_idx]);
        
        if (idle_clock_visible_) {
            DisplayLockGuard lock(this);
            UpdateIdleClockLocked(Lang::Strings::STANDBY);
        }
#endif
    }

    void SetupUI() override {
        SpiLcdDisplay::SetupUI();
        DisplayLockGuard lock(this);
        ApplyRoundScreenUiPadding();
#ifdef CONFIG_BOARD_TYPE_ESP32_C6_SUPER_MINI
        if (status_label_ != nullptr) {
            lv_label_set_long_mode(status_label_, LV_LABEL_LONG_CLIP);
        }
        if (notification_label_ != nullptr) {
            lv_label_set_long_mode(notification_label_, LV_LABEL_LONG_CLIP);
        }
        if (chat_message_label_ != nullptr) {
            lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_CLIP);
        }
#endif
#ifndef CONFIG_BOARD_TYPE_ESP32_C6_SUPER_MINI
        CreateIdleClockLayer();
#endif
        if (high_temp_popup_ != nullptr) {
            lv_obj_move_foreground(high_temp_popup_);
        }
    }

    void SetStatus(const char* status) override {
        if (IsIdleStatus(status)) {
            DisplayLockGuard lock(this);
            ShowIdleClockLocked(true);
            UpdateIdleClockLocked(status);
            return;
        }

        {
            DisplayLockGuard lock(this);
            ShowIdleClockLocked(false);
#ifdef CONFIG_BOARD_TYPE_ESP32_C6_SUPER_MINI
            if (status_label_ != nullptr && std::strcmp(lv_label_get_text(status_label_), status) == 0) {
                if (notification_label_ != nullptr) {
                    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
                }
                return;
            }
#endif
        }
        SpiLcdDisplay::SetStatus(status);
    }

    void UpdateStatusBar(bool update_all = false) override {
        SpiLcdDisplay::UpdateStatusBar(update_all);
        if (!idle_clock_visible_) {
            return;
        }

        DisplayLockGuard lock(this);
        UpdateIdleClockLocked(Lang::Strings::STANDBY);
    }

    void ShowNotification(const char* notification, int duration_ms = 3000) override {
        {
            DisplayLockGuard lock(this);
            ShowIdleClockLocked(false);
        }
        SpiLcdDisplay::ShowNotification(notification, duration_ms);
    }

    void ShowNotification(const std::string& notification, int duration_ms = 3000) override {
        ShowNotification(notification.c_str(), duration_ms);
    }

    void SetEmotion(const char* emotion) override {
        if (idle_clock_visible_) {
            return;
        }
        SpiLcdDisplay::SetEmotion(emotion);
    }

    void SetChatMessage(const char* role, const char* content) override {
        if (idle_clock_visible_ && (content == nullptr || content[0] == '\0')) {
            return;
        }
        if (content != nullptr && content[0] != '\0') {
            DisplayLockGuard lock(this);
            ShowIdleClockLocked(false);
        }
        SpiLcdDisplay::SetChatMessage(role, content);
    }

    void ClearChatMessages() override {
        if (idle_clock_visible_) {
            return;
        }
        SpiLcdDisplay::ClearChatMessages();
    }

    void SetPreviewImage(std::unique_ptr<LvglImage> image) override {
        {
            DisplayLockGuard lock(this);
            ShowIdleClockLocked(false);
        }
        SpiLcdDisplay::SetPreviewImage(std::move(image));
    }

    void SetupHighTempWarningPopup() {
        auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
        auto text_font = lvgl_theme->text_font()->font();
        high_temp_popup_ = lv_obj_create(lv_screen_active());
        lv_obj_set_scrollbar_mode(high_temp_popup_, LV_SCROLLBAR_MODE_OFF);
#if DISPLAY_ROUND_SCREEN
        lv_obj_set_size(high_temp_popup_, LV_HOR_RES * 3 / 5, text_font->line_height * 2);
        lv_obj_align(high_temp_popup_, LV_ALIGN_BOTTOM_MID, 0, -14);
#else
        lv_obj_set_size(high_temp_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
        lv_obj_align(high_temp_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
#endif
        lv_obj_set_style_bg_color(high_temp_popup_, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_radius(high_temp_popup_, 10, 0);
        
        high_temp_label_ = lv_label_create(high_temp_popup_);
        lv_label_set_text(high_temp_label_, "Warning: high temperature");
        lv_obj_set_style_text_color(high_temp_label_, lv_color_white(), 0);
        lv_obj_center(high_temp_label_);
        
        lv_obj_add_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN);
    }

    void UpdateHighTempWarning(float chip_temp, float threshold = 75.0f) {
        if (high_temp_popup_ == nullptr) {
            ESP_LOGW("SuperminiLcdDisplay", "High temp popup not initialized!");
            return;
        }

        if (chip_temp >= threshold) {
            ShowHighTempWarning();
        } else {
            HideHighTempWarning();
        }
    }

    void ShowHighTempWarning() {
        if (high_temp_popup_ && lv_obj_has_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_remove_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(high_temp_popup_);
        }
    }

    void HideHighTempWarning() {
        if (high_temp_popup_ && !lv_obj_has_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN);
        }
    }
};

#endif // SUPERMINI_LCD_DISPLAY_H
