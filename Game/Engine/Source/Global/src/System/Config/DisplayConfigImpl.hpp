#pragma once

#include <cstdint>

namespace ludork::global::system_impl {

class DisplayConfigImpl {
public:
    static void initialize();
    static void shutdown() noexcept;
    static int getFrameRate();
    static void setFrameRate(int value);
    static void saveFrameRate(int value);
    static int getAntiAliasingLevel();
    static void setAntiAliasingLevel(int value);
    static void saveAntiAliasingLevel(int value);
    static bool getVerticalSync();
    static void setVerticalSync(bool value);
    static void saveVerticalSync(bool value);
    static float getScale();
    static float getConfiguredScale();
    static void setScale(float value);
    static void applyScale(float value);
    static void saveScale(float value);

private:
    static float normalizeScale(float scale);
    static int normalizeAntiAliasingLevel(std::int64_t level);
    static float scale_;
    static int frameRate_;
    static int antiAliasingLevel_;
    static bool verticalSync_;
    static bool initialized_;
};

}  // namespace ludork::global::system_impl
