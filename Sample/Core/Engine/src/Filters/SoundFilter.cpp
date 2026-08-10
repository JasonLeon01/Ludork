#include <Filters/SoundFilter.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <random>
#include <vector>

namespace {
struct EchoState {
    std::vector<float> delayBuffer;
    std::size_t bufferIndex = 0;
    std::size_t delayFrames = 1;
};

struct UnderwaterState {
    std::vector<std::size_t> delays;
    std::vector<std::vector<float>> buffers;
    std::vector<std::size_t> indices;
    std::vector<float> previous;
    std::mt19937 random{std::random_device{}()};
    std::uniform_real_distribution<float> unit{0.0f, 1.0f};
    float bubblePhase = 0.0f;
    float waterPhase = 0.0f;
    float envelope = 0.0f;
};
}  // namespace

sf::SoundSource::EffectProcessor echoEffect(float delay, float decay,
                                            float sampleRate) {
    auto state = std::make_shared<EchoState>();
    state->delayFrames = static_cast<std::size_t>(
        std::max(1.0f, std::floor(std::max(0.0f, delay) * sampleRate)));
    state->delayBuffer.resize(state->delayFrames * 2);
    return [state, decay](const float* inputFrames,
                          unsigned int& inputFrameCount, float* outputFrames,
                          unsigned int& outputFrameCount,
                          unsigned int channelCount) {
        const unsigned int frames = std::min(inputFrameCount, outputFrameCount);
        if (channelCount == 0 || outputFrames == nullptr) {
            inputFrameCount = 0;
            outputFrameCount = 0;
            return;
        }
        const std::size_t requiredSize = state->delayFrames * channelCount;
        if (state->delayBuffer.size() < requiredSize) {
            state->delayBuffer.resize(requiredSize);
        }
        for (unsigned int frame = 0; frame < frames; ++frame) {
            for (unsigned int channel = 0; channel < channelCount; ++channel) {
                const std::size_t sampleIndex =
                    static_cast<std::size_t>(frame) * channelCount + channel;
                const std::size_t delayIndex =
                    (state->bufferIndex * channelCount + channel) %
                    state->delayBuffer.size();
                const float input =
                    inputFrames == nullptr ? 0.0f : inputFrames[sampleIndex];
                const float delayed = state->delayBuffer[delayIndex];
                outputFrames[sampleIndex] = input + delayed * decay;
                state->delayBuffer[delayIndex] = input;
            }
            state->bufferIndex = (state->bufferIndex + 1) % state->delayFrames;
        }
        inputFrameCount = frames;
        outputFrameCount = frames;
    };
}

sf::SoundSource::EffectProcessor distortionEffect(float drive,
                                                  float threshold) {
    const float limit = std::max(0.0f, threshold);
    return [drive, limit](const float* inputFrames,
                          unsigned int& inputFrameCount, float* outputFrames,
                          unsigned int& outputFrameCount,
                          unsigned int channelCount) {
        const unsigned int frames = std::min(inputFrameCount, outputFrameCount);
        if (channelCount == 0 || outputFrames == nullptr) {
            inputFrameCount = 0;
            outputFrameCount = 0;
            return;
        }
        const std::size_t sampleCount =
            static_cast<std::size_t>(frames) * channelCount;
        for (std::size_t index = 0; index < sampleCount; ++index) {
            const float input =
                inputFrames == nullptr ? 0.0f : inputFrames[index];
            outputFrames[index] = std::clamp(input * drive, -limit, limit);
        }
        inputFrameCount = frames;
        outputFrameCount = frames;
    };
}

sf::SoundSource::EffectProcessor underwaterEffect(float depth,
                                                  float bubbleIntensity,
                                                  float sampleRate) {
    const float safeRate = std::max(1.0f, sampleRate);
    const float cutoffFrequency = std::max(1.0f, 800.0f - depth * 600.0f);
    const float timeStep = 1.0f / safeRate;
    const float alpha =
        timeStep /
        (1.0f / (cutoffFrequency * 2.0f * std::numbers::pi_v<float>)+timeStep);
    const float bubbleRate = 0.001f + bubbleIntensity * 0.005f;
    const float compressionRatio = 1.0f + depth * 2.0f;
    auto state = std::make_shared<UnderwaterState>();
    for (const unsigned int milliseconds : {43U, 67U, 89U, 127U, 173U}) {
        state->delays.push_back(static_cast<std::size_t>(
            std::max(1.0f, std::floor(safeRate * milliseconds / 1000.0f))));
        state->buffers.emplace_back(state->delays.back() * 2);
        state->indices.push_back(0);
    }
    return [state, depth, bubbleIntensity, safeRate, timeStep, alpha,
            bubbleRate, compressionRatio](
               const float* inputFrames, unsigned int& inputFrameCount,
               float* outputFrames, unsigned int& outputFrameCount,
               unsigned int channelCount) {
        const unsigned int frames = std::min(inputFrameCount, outputFrameCount);
        if (channelCount == 0 || outputFrames == nullptr) {
            inputFrameCount = 0;
            outputFrameCount = 0;
            return;
        }
        if (state->previous.size() < channelCount) {
            state->previous.resize(channelCount);
        }
        for (std::size_t index = 0; index < state->delays.size(); ++index) {
            const std::size_t requiredSize =
                state->delays[index] * channelCount;
            if (state->buffers[index].size() < requiredSize) {
                state->buffers[index].resize(requiredSize);
            }
        }
        for (unsigned int frame = 0; frame < frames; ++frame) {
            const float modulation =
                1.0f + 0.05f * depth * std::sin(state->waterPhase);
            state->waterPhase =
                std::fmod(state->waterPhase + 2.0f * std::numbers::pi_v<float> *
                                                  0.5f / safeRate,
                          2.0f * std::numbers::pi_v<float>);
            float bubble = 0.0f;
            if (state->unit(state->random) < bubbleRate) {
                const float frequency =
                    200.0f + state->unit(state->random) * 800.0f;
                const float amplitude =
                    (state->unit(state->random) * 0.5f + 0.5f) *
                    bubbleIntensity * 0.1f;
                bubble = amplitude * std::sin(state->bubblePhase * frequency) *
                         std::exp(-state->bubblePhase * 10.0f);
            }
            state->bubblePhase = std::fmod(state->bubblePhase + timeStep, 1.0f);
            for (unsigned int channel = 0; channel < channelCount; ++channel) {
                const std::size_t sampleIndex =
                    static_cast<std::size_t>(frame) * channelCount + channel;
                float signal =
                    (inputFrames == nullptr ? 0.0f : inputFrames[sampleIndex]) *
                    modulation;
                const float absolute = std::abs(signal);
                state->envelope =
                    absolute +
                    (state->envelope - absolute) *
                        (absolute > state->envelope ? 0.95f : 0.999f);
                if (state->envelope > 0.3f) {
                    const float gain =
                        0.3f + (state->envelope - 0.3f) / compressionRatio;
                    signal *= gain / state->envelope;
                }
                const float filtered =
                    alpha * signal + (1.0f - alpha) * state->previous[channel];
                state->previous[channel] = filtered;
                float reverb = 0.0f;
                for (std::size_t index = 0; index < state->delays.size();
                     ++index) {
                    std::vector<float>& buffer = state->buffers[index];
                    const std::size_t offset =
                        (state->indices[index] * channelCount + channel) %
                        buffer.size();
                    reverb += buffer[offset] * (0.2f + depth * 0.3f);
                    buffer[offset] =
                        filtered + buffer[offset] * (0.7f - depth * 0.2f);
                }
                const float channelBubble =
                    channel == 0
                        ? bubble
                        : bubble * (0.7f + state->unit(state->random) * 0.6f);
                const float output =
                    (filtered + reverb * 0.4f + channelBubble) *
                    (1.0f - depth * 0.4f);
                outputFrames[sampleIndex] = std::clamp(output, -0.8f, 0.8f);
            }
            for (std::size_t index = 0; index < state->indices.size();
                 ++index) {
                state->indices[index] =
                    (state->indices[index] + 1) % state->delays[index];
            }
        }
        inputFrameCount = frames;
        outputFrameCount = frames;
    };
}
