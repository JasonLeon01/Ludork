#pragma once

namespace ludork::global::system_impl {

class GraphicsConfigImpl {
public:
    static void initialize();
    static void initializeRenderScale(float configuredScale);
    static void shutdown() noexcept;
    static float getMaximumRenderScale();
    static void setMaximumRenderScale(float value);
    static void saveMaximumRenderScale(float value);
    static float getLightingRenderScale();
    static void setLightingRenderScale(float value);
    static void saveLightingRenderScale(float value);

private:
    static float normalizeMaximumRenderScale(float scale);
    static float normalizeLightingRenderScale(float scale);
    static float maximumRenderScale_;
    static float lightingRenderScale_;
    static bool initialized_;
};

}  // namespace ludork::global::system_impl
