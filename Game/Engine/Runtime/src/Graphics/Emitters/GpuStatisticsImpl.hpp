#pragma once

#include "GpuResourcesImpl.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace ludork::runtime::graphics {

class GpuStatisticsImpl {
public:
    struct Sample {
        std::uint32_t aliveCount;
        double sampleTime;
    };

    explicit GpuStatisticsImpl(std::shared_ptr<GpuResourcesImpl> resources);
    ~GpuStatisticsImpl();
    GpuStatisticsImpl(const GpuStatisticsImpl&) = delete;
    GpuStatisticsImpl& operator=(const GpuStatisticsImpl&) = delete;

    bool available() const;
    bool submit(const std::vector<std::pair<unsigned int, int>>& stateBuffers,
                double simulationTime, bool force = false);
    std::optional<Sample> poll();

private:
    struct StateGuard {
        explicit StateGuard(GpuApi& api);
        ~StateGuard();

        GpuApi& api;
        GpuStateGuard base;
        int drawFramebuffer = 0;
        int readFramebuffer = 0;
        int packBuffer = 0;
        int unpackBuffer = 0;
        std::array<int, 4> packSettings{};
        std::array<float, 4> clearColour{};
        std::array<unsigned char, 4> colourMask{};
        std::array<bool, 7> enables{};
        bool srgb = false;
        bool alphaTest = false;
        bool pointSmooth = false;
        bool pointSize = false;
        bool logicOp = false;
    };

    void reserve(int size);
    void attach(unsigned int texture, int size);
    void destroy() noexcept;

    std::shared_ptr<GpuResourcesImpl> resources_;
    unsigned int maskProgram_ = 0;
    unsigned int reductionProgram_ = 0;
    int maskSide_ = -1;
    int maskBase_ = -1;
    int reductionSide_ = -1;
    int reductionSource_ = -1;
    unsigned int framebuffer_ = 0;
    std::array<unsigned int, 2> textures_{};
    unsigned int readback_ = 0;
    int size_ = 0;
    void* fence_ = nullptr;
    double pendingTime_ = 0;
    std::optional<double> lastSubmitTime_;
    std::optional<Sample> zeroSample_;
};

}  // namespace ludork::runtime::graphics
