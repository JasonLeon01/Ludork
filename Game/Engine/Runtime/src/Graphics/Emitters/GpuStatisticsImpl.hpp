#pragma once

#include "GpuResourcesImpl.hpp"

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
    using StateBuffer = std::pair<std::shared_ptr<sf::rhi::Buffer>, int>;

    explicit GpuStatisticsImpl(std::shared_ptr<GpuResourcesImpl> resources);
    ~GpuStatisticsImpl();
    GpuStatisticsImpl(const GpuStatisticsImpl&) = delete;
    GpuStatisticsImpl& operator=(const GpuStatisticsImpl&) = delete;

    bool available() const;
    bool submit(const std::vector<StateBuffer>& stateBuffers,
                double simulationTime, bool force = false);
    std::optional<Sample> poll();

private:
    static constexpr double MinimumSampleInterval = 0.25;

    void reserve(int size);

    std::shared_ptr<GpuResourcesImpl> resources_;
    std::shared_ptr<sf::Shader> maskShader_;
    std::shared_ptr<sf::Shader> reductionShader_;
    std::shared_ptr<sf::rhi::ShaderProgram> maskProgram_;
    std::shared_ptr<sf::rhi::ShaderProgram> reductionProgram_;
    std::vector<std::shared_ptr<sf::rhi::Texture>> textures_;
    std::vector<std::shared_ptr<sf::rhi::RenderTarget>> targets_;
    std::shared_ptr<sf::rhi::Readback> readback_;
    int size_ = 0;
    double pendingTime_ = 0;
    std::optional<double> lastSubmitTime_;
    std::optional<Sample> zeroSample_;
};

}  // namespace ludork::runtime::graphics
