#include "RuntimeAudioProcessorImpl.hpp"
#include <Runtime/ScriptStore.hpp>
#include <Standard.hpp>
#include <LuaCallbackCodec.hpp>
#include <LuaStateLifecycle.hpp>
#include <LuaSF.hpp>
#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <array>
#include <stdexcept>
#include <utility>

namespace ludork::runtime {
namespace {
constexpr std::size_t DeferredErrorCapacity = 512;

void closeState(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    LuaSF_quiesce_state(state);
    LuaSF_shutdown_state(state);
    lua_close(state);
}

void requireProtectedResult(const lua_glue::CallResult& result,
                            const std::string& operation) {
    if (result.valid()) {
        return;
    }
    const std::string error = result.error();
    throw std::runtime_error(operation + ": " + error.c_str());
}

}  // namespace

AudioControlImpl::AudioControlImpl(AudioProcessorControl control)
    : control_(std::move(control)) {}

bool AudioControlImpl::isCancelled() const {
    return control_.isCancelled();
}

void AudioControlImpl::beginTail() {
    control_.beginTail();
}

void AudioControlImpl::finishTail() {
    control_.finishTail();
}

AudioProcessor::Impl::Impl(AudioProcessorOptions options) {
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
        lua_glue::StateView lua(state_);
        lua_glue::Table package = lua["package"];
        package["path"] = options.packagePath;
        ludork::standard::initializeMath(state_);
        ludork::runtime::scriptStore().registerPreloadedModules(state_);

        auto audioControl = lua_glue::BindClass<AudioControlImpl>(
            lua.globals(), "LudorkAudioEffectControl");
        lua_glue::BindMethod<bool>(audioControl, "isCancelled",
                                   &AudioControlImpl::isCancelled);
        lua_glue::BindMethod<void>(audioControl, "beginTail",
                                   &AudioControlImpl::beginTail);
        lua_glue::BindMethod<void>(audioControl, "finishTail",
                                   &AudioControlImpl::finishTail);

        lua_glue::Function require = lua["require"];
        lua_glue::CallResult moduleResult = require(options.moduleName);
        requireProtectedResult(moduleResult,
                               "Failed to load " + options.moduleName);
        const lua_glue::Object moduleObject = moduleResult;
        if (!moduleObject.is<lua_glue::Table>()) {
            throw std::runtime_error(options.moduleName +
                                     " did not return a module table");
        }
        const lua_glue::Table module = moduleObject.as<lua_glue::Table>();
        const lua_glue::Object getObject = module[options.resolverName];
        if (!getObject.is<lua_glue::Function>()) {
            throw std::runtime_error(options.moduleName + "." +
                                     options.resolverName + " is unavailable");
        }
        const lua_glue::Function get = getObject.as<lua_glue::Function>();
        lua_glue::CallResult attacherResult = get(options.effectName);
        requireProtectedResult(
            attacherResult,
            "Failed to resolve audio effect " + options.effectName);
        const lua_glue::Object attacherObject = attacherResult;
        if (!attacherObject.is<lua_glue::Function>()) {
            throw std::runtime_error(
                options.moduleName + "." + options.resolverName +
                " did not return an attacher for " + options.effectName);
        }
        const lua_glue::Function attacher =
            attacherObject.as<lua_glue::Function>();
        lua_glue::CallResult processorResult = attacher(
            lua_glue::nil,
            std::make_shared<AudioControlImpl>(std::move(options.control)),
            options.sampleRate);
        requireProtectedResult(
            processorResult,
            "Failed to create audio effect " + options.effectName);
        const lua_glue::Object processorObject = processorResult;
        processor_ = lua_sf::callback::from_object<
            sf::SoundSource::EffectProcessor,
            lua_sf::callback::InterleavedFloatTransformCodec>(
            processorObject,
            lua_sf::callback::CallbackOptions{
                options.moduleName + "." + options.effectName, false});
        lua_gc(state_, LUA_GCSTOP);
    } catch (...) {
        processor_ = {};
        closeState(state_);
        state_ = nullptr;
        throw;
    }
}

AudioProcessor::Impl::~Impl() {
    processor_ = {};
    closeState(state_);
}

void AudioProcessor::Impl::process(const float* inputFrames,
                                   unsigned int& inputFrameCount,
                                   float* outputFrames,
                                   unsigned int& outputFrameCount,
                                   unsigned int frameChannelCount) noexcept {
    processor_(inputFrames, inputFrameCount, outputFrames, outputFrameCount,
               frameChannelCount);
}

std::optional<std::string> AudioProcessor::Impl::takeDeferredError() const {
    std::array<char, DeferredErrorCapacity> buffer{};
    if (state_ == nullptr || LuaSF_take_deferred_callback_error(
                                 state_, buffer.data(), buffer.size()) == 0) {
        return std::nullopt;
    }
    return std::string(buffer.data());
}

AudioProcessor::AudioProcessor(AudioProcessorOptions options)
    : impl_(std::make_unique<Impl>(std::move(options))) {}
AudioProcessor::~AudioProcessor() = default;
void AudioProcessor::process(const float* inputFrames,
                             unsigned int& inputFrameCount, float* outputFrames,
                             unsigned int& outputFrameCount,
                             unsigned int frameChannelCount) noexcept {
    impl_->process(inputFrames, inputFrameCount, outputFrames, outputFrameCount,
                   frameChannelCount);
}
std::optional<std::string> AudioProcessor::takeDeferredError() const {
    return impl_->takeDeferredError();
}

}  // namespace ludork::runtime
