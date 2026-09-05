#pragma once

#include <RuntimeApi.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ludork::runtime {

struct AudioProcessorControl {
    std::function<bool()> isCancelled;
    std::function<void()> beginTail;
    std::function<void()> finishTail;
};

struct AudioProcessorOptions {
    std::string packagePath;
    std::string moduleName;
    std::string resolverName;
    std::string effectName;
    std::string helperModuleName;
    std::string clampName;
    std::function<double(double, double, double)> clamp;
    AudioProcessorControl control;
    std::uint32_t sampleRate = 0;
};

class LUDORK_RUNTIME_API AudioProcessor final {
public:
    explicit AudioProcessor(AudioProcessorOptions options);
    ~AudioProcessor();
    AudioProcessor(const AudioProcessor&) = delete;
    AudioProcessor& operator=(const AudioProcessor&) = delete;

    void process(const float* inputFrames, unsigned int& inputFrameCount,
                 float* outputFrames, unsigned int& outputFrameCount,
                 unsigned int frameChannelCount) noexcept;
    std::optional<std::string> takeDeferredError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::runtime
