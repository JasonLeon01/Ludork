#include "AudioEffectLuaRuntime.hpp"

#include <Manager/ManagedAudioSource.hpp>
#include <Utils/Math.hpp>

#include <LuaCallbackCodec.hpp>
#include <LuaStateLifecycle.hpp>
#include <LuaSF.hpp>
#include <Runtime/ScriptStore.hpp>
#include <luasf_sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <array>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ludork::global::audio {

namespace {

constexpr std::size_t DeferredErrorCapacity = 512;

class LuaAudioEffectProcessor;

std::mutex runtimeMutex;
std::string runtimePackagePath;
bool runtimeInitialized = false;
std::vector<std::weak_ptr<LuaAudioEffectProcessor>> processors;
std::deque<std::string> pendingErrors;

void closeState(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    LuaSF_quiesce_state(state);
    LuaSF_shutdown_state(state);
    lua_close(state);
}

void requireProtectedResult(const sol::protected_function_result& result,
                            const std::string& operation) {
    if (result.valid()) {
        return;
    }
    const sol::error error = result;
    throw std::runtime_error(operation + ": " + error.what());
}

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
                            const std::string& packagePath) {
        state_ = luaL_newstate();
        if (state_ == nullptr) {
            throw std::runtime_error(
                "Unable to create the isolated audio-effect Lua state");
        }
        try {
            luaL_openlibs(state_);
            if (LuaSF_initialize_state(state_) != 0) {
                throw std::runtime_error(
                    "Unable to initialize the isolated audio-effect Lua state");
            }
            lua_sf::LuaStateExecutionScope execution(state_);
            if (!execution.active()) {
                throw std::runtime_error(
                    "Unable to enter the isolated audio-effect Lua state");
            }
            sol::state_view lua(state_);
            sol::table package = lua["package"];
            package["path"] = packagePath;
            sol::table loaded = package["loaded"];
            sol::table engine = lua.create_table();
            engine.set_function("Clamp", &clampNumber);
            loaded["Engine"] = engine;
            ludork::runtime::scriptStore().registerPreloadedModules(state_);

            lua.new_usertype<AudioEffectControl>(
                "LudorkAudioEffectControl", sol::no_constructor, "isCancelled",
                &AudioEffectControl::isCancelled, "beginTail",
                &AudioEffectControl::beginTail, "finishTail",
                &AudioEffectControl::finishTail);
            lua_sf::register_external_usertype<AudioEffectControl>(lua);

            sol::protected_function require = lua["require"];
            sol::protected_function_result moduleResult =
                require("Source.AudioEffects");
            requireProtectedResult(moduleResult,
                                   "Failed to load Source.AudioEffects");
            const sol::object moduleObject = moduleResult;
            if (!moduleObject.is<sol::table>()) {
                throw std::runtime_error(
                    "Source.AudioEffects did not return a module table");
            }
            const sol::table module = moduleObject.as<sol::table>();
            const sol::object getObject = module["Get"];
            if (!getObject.is<sol::protected_function>()) {
                throw std::runtime_error(
                    "Source.AudioEffects.Get is unavailable");
            }
            const sol::protected_function get =
                getObject.as<sol::protected_function>();
            sol::protected_function_result attacherResult = get(name);
            requireProtectedResult(attacherResult,
                                   "Failed to resolve audio effect " + name);
            const sol::object attacherObject = attacherResult;
            if (!attacherObject.is<sol::protected_function>()) {
                throw std::runtime_error(
                    "Source.AudioEffects.Get did not "
                    "return an attacher for " +
                    name);
            }
            const sol::protected_function attacher =
                attacherObject.as<sol::protected_function>();
            sol::protected_function_result processorResult = attacher(
                sol::lua_nil, lua_sf::wrapLuaSharedObject(control), sampleRate);
            requireProtectedResult(processorResult,
                                   "Failed to create audio effect " + name);
            const sol::object processorObject = processorResult;
            processor_ = lua_sf::callback::from_object<
                sf::SoundSource::EffectProcessor,
                lua_sf::callback::InterleavedFloatTransformCodec>(
                processorObject, lua_sf::callback::CallbackOptions{
                                     "Source.AudioEffects." + name, false});
            lua_gc(state_, LUA_GCSTOP);
        } catch (...) {
            processor_ = {};
            closeState(state_);
            state_ = nullptr;
            throw;
        }
    }

    ~LuaAudioEffectProcessor() {
        processor_ = {};
        if (const std::optional<std::string> error = takeDeferredError();
            error.has_value()) {
            enqueueDeferredError(*error);
        }
        closeState(state_);
    }

    LuaAudioEffectProcessor(const LuaAudioEffectProcessor&) = delete;
    LuaAudioEffectProcessor& operator=(const LuaAudioEffectProcessor&) = delete;

    void process(const float* inputFrames, unsigned int& inputFrameCount,
                 float* outputFrames, unsigned int& outputFrameCount,
                 unsigned int frameChannelCount) noexcept {
        processor_(inputFrames, inputFrameCount, outputFrames, outputFrameCount,
                   frameChannelCount);
    }

    [[nodiscard]] std::optional<std::string> takeDeferredError() const {
        std::array<char, DeferredErrorCapacity> buffer{};
        if (state_ == nullptr ||
            LuaSF_take_deferred_callback_error(state_, buffer.data(),
                                               buffer.size()) == 0) {
            return std::nullopt;
        }
        return std::string(buffer.data());
    }

private:
    lua_State* state_ = nullptr;
    sf::SoundSource::EffectProcessor processor_;
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
