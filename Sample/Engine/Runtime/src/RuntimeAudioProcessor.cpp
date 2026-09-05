#include <Runtime/RuntimeAudioProcessor.hpp>
#include <Runtime/ScriptStore.hpp>
#include <LuaCallbackCodec.hpp>
#include <LuaStateLifecycle.hpp>
#include <LuaSF.hpp>
#include <luasf_sol.hpp>

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

class AudioControl {
public:
    explicit AudioControl(AudioProcessorControl control)
        : control_(std::move(control)) {}
    bool isCancelled() const {
        return control_.isCancelled();
    }
    void beginTail() {
        control_.beginTail();
    }
    void finishTail() {
        control_.finishTail();
    }

private:
    AudioProcessorControl control_;
};

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

}  // namespace

struct AudioProcessor::Impl {
public:
    explicit Impl(AudioProcessorOptions options) {
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
            package["path"] = options.packagePath;
            sol::table loaded = package["loaded"];
            sol::table engine = lua.create_table();
            engine.set_function(options.clampName, options.clamp);
            loaded[options.helperModuleName] = engine;
            ludork::runtime::scriptStore().registerPreloadedModules(state_);

            lua.new_usertype<AudioControl>(
                "LudorkAudioEffectControl", sol::no_constructor, "isCancelled",
                &AudioControl::isCancelled, "beginTail",
                &AudioControl::beginTail, "finishTail",
                &AudioControl::finishTail);
            lua_sf::register_external_usertype<AudioControl>(lua);

            sol::protected_function require = lua["require"];
            sol::protected_function_result moduleResult =
                require(options.moduleName);
            requireProtectedResult(moduleResult,
                                   "Failed to load " + options.moduleName);
            const sol::object moduleObject = moduleResult;
            if (!moduleObject.is<sol::table>()) {
                throw std::runtime_error(options.moduleName +
                                         " did not return a module table");
            }
            const sol::table module = moduleObject.as<sol::table>();
            const sol::object getObject = module[options.resolverName];
            if (!getObject.is<sol::protected_function>()) {
                throw std::runtime_error(options.moduleName + "." +
                                         options.resolverName +
                                         " is unavailable");
            }
            const sol::protected_function get =
                getObject.as<sol::protected_function>();
            sol::protected_function_result attacherResult =
                get(options.effectName);
            requireProtectedResult(
                attacherResult,
                "Failed to resolve audio effect " + options.effectName);
            const sol::object attacherObject = attacherResult;
            if (!attacherObject.is<sol::protected_function>()) {
                throw std::runtime_error(
                    options.moduleName + "." + options.resolverName +
                    " did not return an attacher for " + options.effectName);
            }
            const sol::protected_function attacher =
                attacherObject.as<sol::protected_function>();
            sol::protected_function_result processorResult = attacher(
                sol::lua_nil,
                lua_sf::wrapLuaSharedObject(
                    std::make_shared<AudioControl>(std::move(options.control))),
                options.sampleRate);
            requireProtectedResult(
                processorResult,
                "Failed to create audio effect " + options.effectName);
            const sol::object processorObject = processorResult;
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

    ~Impl() {
        processor_ = {};
        closeState(state_);
    }

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
