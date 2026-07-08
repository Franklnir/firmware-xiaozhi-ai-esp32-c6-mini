import sys

path = r'c:\Users\frank\Music\xiaozhi-esp32-main\main\boards\esp32-c6-super-mini\supermini_lcd_display.h'
with open(path, 'r', encoding='utf-8') as f:
    code = f.read()

# 1. Update const declarations
code = code.replace('static constexpr int kIdleCanvasSize = 240;', 'static constexpr int kIdleCanvasWidth = 240;\n    static constexpr int kIdleCanvasHeight = 280;')
code = code.replace('static constexpr int kIdleBaseY = 20;', 'static constexpr int kIdleBaseY = 0;')
code = code.replace('static constexpr int kWatchCenter = 120;', 'static constexpr int kWatchCenterX = 120;\n    static constexpr int kWatchCenterY = 140;')

# 2. Update buffer allocation
code = code.replace('idle_background_stride_ = lv_draw_buf_width_to_stride(kIdleCanvasSize', 'idle_background_stride_ = lv_draw_buf_width_to_stride(kIdleCanvasWidth')
code = code.replace('idle_background_stride_ * kIdleCanvasSize', 'idle_background_stride_ * kIdleCanvasHeight')
code = code.replace('kIdleCanvasSize, kIdleCanvasSize', 'kIdleCanvasWidth, kIdleCanvasHeight')
code = code.replace('lv_obj_set_pos(idle_canvas_, 0, 20);', 'lv_obj_set_pos(idle_canvas_, 0, 0);')

# 3. Update drawing boundaries
code = code.replace('x >= kIdleCanvasSize', 'x >= kIdleCanvasWidth')
code = code.replace('y >= kIdleCanvasSize', 'y >= kIdleCanvasHeight')
code = code.replace('max_x = std::min(kIdleCanvasSize - 1', 'max_x = std::min(kIdleCanvasWidth - 1')
code = code.replace('max_y = std::min(kIdleCanvasSize - 1', 'max_y = std::min(kIdleCanvasHeight - 1')

code = code.replace('for (int y = 0; y < kIdleCanvasSize; ++y) {', 'for (int y = 0; y < kIdleCanvasHeight; ++y) {')
code = code.replace('for (int yy = y; yy < y + h; ++yy) {\n            if (yy < 0 || yy >= kIdleCanvasSize) {', 'for (int yy = y; yy < y + h; ++yy) {\n            if (yy < 0 || yy >= kIdleCanvasHeight) {')
code = code.replace('for (int x = 0; x < kIdleCanvasSize; ++x) {', 'for (int x = 0; x < kIdleCanvasWidth; ++x) {')
code = code.replace('for (int xx = x; xx < x + w; ++xx) {\n                if (xx >= 0 && xx < kIdleCanvasSize) {', 'for (int xx = x; xx < x + w; ++xx) {\n                if (xx >= 0 && xx < kIdleCanvasWidth) {')

# 4. Analog watch hands
code = code.replace('kWatchCenter - 6, kWatchCenter - 6', 'kWatchCenterX - 6, kWatchCenterY - 6')
code = code.replace('kWatchCenter + static_cast', 'kWatchCenterX + static_cast')
code = code.replace('points[0].x = kWatchCenter;', 'points[0].x = kWatchCenterX;')
code = code.replace('points[0].y = kWatchCenter;', 'points[0].y = kWatchCenterY;')
code = code.replace('points[1].x = kWatchCenter', 'points[1].x = kWatchCenterX')
code = code.replace('points[1].y = kWatchCenter', 'points[1].y = kWatchCenterY')

# Update max/min for x/y bounds of analog watch
code = code.replace('x = std::min(x, kWatchCenter)', 'x = std::min(x, kWatchCenterX)')
code = code.replace('x2 = std::max(x2, kWatchCenter)', 'x2 = std::max(x2, kWatchCenterX)')
code = code.replace('y = std::min(y, kWatchCenter)', 'y = std::min(y, kWatchCenterY)')
code = code.replace('y2 = std::max(y2, kWatchCenter)', 'y2 = std::max(y2, kWatchCenterY)')

# 5. Render stretching logic
code = code.replace('int src_y = y * height / kIdleCanvasHeight;', 'int src_y = y * height / kIdleCanvasHeight;') 
code = code.replace('int src_y = yy * 240 / kIdleCanvasHeight;', 'int src_y = yy * 240 / kIdleCanvasHeight;')

old_rw1 = '''        for (int y = 0; y < kIdleCanvasHeight; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   y * idle_background_stride_);
            const unsigned char* src = pixels + (y * bytes_per_row);'''
new_rw1 = '''        for (int y = 0; y < kIdleCanvasHeight; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   y * idle_background_stride_);
            int src_y = y * height / kIdleCanvasHeight;
            const unsigned char* src = pixels + (src_y * bytes_per_row);'''
code = code.replace(old_rw1, new_rw1)

old_rr1 = '''        for (int y = 0; y < kIdleCanvasHeight; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   y * idle_background_stride_);
            std::memcpy(row, face_data + (y * kIdleCanvasSize), kIdleCanvasSize * sizeof(uint16_t));
        }'''
new_rr1 = '''        for (int y = 0; y < kIdleCanvasHeight; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   y * idle_background_stride_);
            int src_y = y * height / kIdleCanvasHeight;
            std::memcpy(row, face_data + (src_y * width), width * sizeof(uint16_t));
        }'''
old_rr2 = '''        for (int y = 0; y < kIdleCanvasHeight; ++y) {
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   y * idle_background_stride_);
            std::memcpy(row, face_data + (y * kIdleCanvasWidth), kIdleCanvasWidth * sizeof(uint16_t));
        }'''
code = code.replace(old_rr1, new_rr1)
code = code.replace(old_rr2, new_rr1)

old_ew = '''    void EraseWatchByIrsyad2TextRectLocked(int x, int y, int w, int h, int sample_y) {
        if (idle_background_buffer_ == nullptr || idle_background_stride_ == 0) {
            return;
        }

        sample_y = std::clamp(sample_y, 0, kIdleCanvasHeight - 1);
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
    }'''
new_ew = '''    void EraseWatchByIrsyad2TextRectLocked(int x, int y, int w, int h, int sample_y) {
        if (idle_background_buffer_ == nullptr || idle_background_stride_ == 0) {
            return;
        }

        sample_y = std::clamp(sample_y, 0, 240 - 1);
        for (int yy = y; yy < y + h; ++yy) {
            if (yy < 0 || yy >= kIdleCanvasHeight) {
                continue;
            }
            auto row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(idle_background_buffer_) +
                                                   yy * idle_background_stride_);
            for (int xx = x; xx < x + w; ++xx) {
                if (xx >= 0 && xx < kIdleCanvasWidth) {
                    row[xx] = epd_bitmap_watchbyirsyad2[sample_y * 240 + xx];
                }
            }
        }
    }'''
code = code.replace(old_ew, new_ew)

old_ua = '''        int sample_y = y - 20;
        EraseWatchByIrsyad2TextRectLocked(x, y, w, h, sample_y);'''
new_ua = '''        int sample_y = y * 240 / kIdleCanvasHeight;
        EraseWatchByIrsyad2TextRectLocked(x, y, w, h, sample_y);'''
code = code.replace(old_ua, new_ua)

code = code.replace('EraseWatchByIrsyad2TextRectLocked(102, 75, 36, 17, 92);', 'int mapped_y = 75 * kIdleCanvasHeight / 240;\n        int sample_y = 92 * kIdleCanvasHeight / 240;\n        EraseWatchByIrsyad2TextRectLocked(102, mapped_y, 36, 17, sample_y);')

with open(path, 'w', encoding='utf-8') as f:
    f.write(code)

print("Done")
