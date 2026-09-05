#include "AudioEffectLuaRuntime.hpp"

#include <Manager/ManagedAudioSource.hpp>
#include <Utils/Math.hpp>
#include <Runtime/RuntimeAudioProcessor.hpp>

extern "C" {
#include <lua.h>
}

#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ludork::global::audio {

namespace {

class LuaAudioEffectProcessor;

std::mutex runtimeMutex;
std::string runtimePackagePath;
bool runtimeInitialized = false;
std::vector<std::weak_ptr<LuaAudioEffectProcessor>> processors;
std::deque<std::string> pendingErrors;

void enqueueDeferredError(std::string error) noexcept {
    if (error.empty()) {
        return;
    }
    try {
        const std::lock_guard<std::mutex> lock(runtimeMutex);
        pendingErrors.push_back(std::move(error));
    } catch (...) {}
}

class LuaAudioEffectProcessor final {
public:
    LuaAudioEffectProcessor(const std::string& name,
                            const std::shared_ptr<AudioEffectControl>& control,
                            std::uint32_t sampleRate,
                            const std::string& packagePath)
        : processor_(ludork::runtime::AudioProcessorOptions{
              packagePath,
              "Source.AudioEffects",
              "Get",
              name,
              "Engine",
              "Clamp",
              &clampNumber,
              {[control] {
                   return control->isCancelled();
               },
               [control] {
                   control->beginTail();
               },
               [control] {
                   control->finishTail();
               }},
              sampleRate}) {}

    ~LuaAudioEffectProcessor() {
        if (const std::optional<std::string> error = takeDeferredError();
            error.has_value()) {
            enqueueDeferredError(*error);
        }
    }
    LuaAudioEffectProcessor(const LuaAudioEffectProcessor&) = delete;
    LuaAudioEffectProcessor& operator=(const LuaAudioEffectProcessor&) = delete;

    void process(const float* inputFrames, unsigned int& inputFrameCount,
                 float* outputFrames, unsigned int& outputFrameCount,
                 unsigned int frameChannelCount) noexcept {
        processor_.process(inputFrames, inputFrameCount, outputFrames,
                           outputFrameCount, frameChannelCount);
    }
    std::optional<std::string> takeDeferredError() const {
        return processor_.takeDeferredError();
    }

private:
    ludork::runtime::AudioProcessor processor_;
};

std::string packagePathFrom(lua_State* state) {
    const int stackBase = lua_gettop(state);
    lua_getglobal(state, "package");
    if (!lua_istable(state, -1)) {
        lua_settop(state, stackBase);
        throw std::runtime_error("Lua package table is unavailable");
    }
    lua_getfield(state, -1, "path");
    std::size_t length = 0;
    const char* value = lua_tolstring(state, -1, &length);
    if (value == nullptr) {
        lua_settop(state, stackBase);
        throw std::runtime_error("Lua package.path is unavailable");
    }
    const std::string result(value, length);
    lua_settop(state, stackBase);
    return result;
}

}  // namespace

void initializeAudioEffectLuaRuntime(lua_State* mainState) {
    if (mainState == nullptr) {
        throw std::invalid_argument(
            "Audio-effect Lua runtime requires the main Lua state");
    }
    const std::string packagePath = packagePathFrom(mainState);
    const std::lock_guard<std::mutex> lock(runtimeMutex);
    runtimePackagePath = packagePath;
    processors.clear();
    pendingErrors.clear();
    runtimeInitialized = true;
}

void shutdownAudioEffectLuaRuntime() noexcept {
    const std::lock_guard<std::mutex> lock(runtimeMutex);
    runtimeInitialized = false;
    runtimePackagePath.clear();
    processors.clear();
    pendingErrors.clear();
}

sf::SoundSource::EffectProcessor createLuaAudioEffectProcessor(
    const std::string& name, const std::shared_ptr<AudioEffectControl>& control,
    std::uint32_t sampleRate) {
    if (control == nullptr) {
        throw std::invalid_argument("Audio effect control is missing");
    }
    std::string packagePath;
    {
        const std::lock_guard<std::mutex> lock(runtimeMutex);
        if (!runtimeInitialized) {
            throw std::runtime_error(
                "Audio-effect Lua runtime is not initialized");
        }
        packagePath = runtimePackagePath;
    }
    const std::shared_ptr<LuaAudioEffectProcessor> processor =
        std::make_shared<LuaAudioEffectProcessor>(name, control, sampleRate,
                                                  packagePath);
    {
        const std::lock_guard<std::mutex> lock(runtimeMutex);
        if (!runtimeInitialized) {
            throw std::runtime_error("Audio-effect Lua runtime is stopping");
        }
        processors.push_back(processor);
    }
    return [processor](const float* inputFrames, unsigned int& inputFrameCount,
                       float* outputFrames, unsigned int& outputFrameCount,
                       unsigned int frameChannelCount) noexcept {
        processor->process(inputFrames, inputFrameCount, outputFrames,
                           outputFrameCount, frameChannelCount);
    };
}

void throwDeferredAudioEffectError() {
    std::vector<std::shared_ptr<LuaAudioEffectProcessor>> activeProcessors;
    std::optional<std::string> pending;
    {
        const std::lock_guard<std::mutex> lock(runtimeMutex);
        if (!pendingErrors.empty()) {
            pending = std::move(pendingErrors.front());
            pendingErrors.pop_front();
        }
        for (auto iterator = processors.begin();
             iterator != processors.end();) {
            if (std::shared_ptr<LuaAudioEffectProcessor> processor =
                    iterator->lock()) {
                activeProcessors.push_back(std::move(processor));
                ++iterator;
            } else {
                iterator = processors.erase(iterator);
            }
        }
    }
    if (pending.has_value()) {
        throw std::runtime_error(*pending);
    }
    for (const std::shared_ptr<LuaAudioEffectProcessor>& processor :
         activeProcessors) {
        if (const std::optional<std::string> error =
                processor->takeDeferredError();
            error.has_value()) {
            throw std::runtime_error(*error);
        }
    }
}

}  // namespace ludork::global::audio
