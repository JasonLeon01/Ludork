#include <GlobalRuntimeApi.hpp>

#include "PerformanceProfiler.hpp"

#include <CustomParticles/CommonTipController.hpp>
#include <EditorCommandServices.hpp>
#include <Fog/FogController.hpp>
#include <Manager/ActorAudioBridge.hpp>
#include <Manager/AudioManager.hpp>
#include <Manager/FontManager.hpp>
#include <Manager/ShaderManager.hpp>
#include <Manager/TextureManager.hpp>
#include <Manager/TimeManager.hpp>
#include <Manager/UiAudioBridge.hpp>
#include <RuntimeSession.hpp>
#include <System.hpp>
#include <SystemConfigBase.hpp>
#include <UIManager.hpp>
#include <VideoPlayback.hpp>
#include <Weather/WeatherController.hpp>

extern "C" {
#include <lua.h>
}

namespace {

ludork::global::RuntimeLaunchOptions launchOptions;

void registerEditorCommands(lua_State* state) {
    const int stackBase = lua_gettop(state);
    lua_getglobal(state, "GlobalCore");
    if (lua_istable(state, -1)) {
        ludork::standard::registerEditorCommandEnvironment(state, "GlobalCore",
                                                           -1);
        lua_getfield(state, -1, "System");
        if (lua_istable(state, -1)) {
            lua_getfield(state, -1, "exit");
            if (lua_isfunction(state, -1)) {
                ludork::standard::registerEditorCommandShutdownHandler(state,
                                                                       -1);
            }
        }
    }
    lua_settop(state, stackBase);
    ludork::standard::registerEditorCommandBoolControlHandler(
        state, "performanceMonitor", &PerformanceProfiler::setEnabled);
}

void unregisterEditorCommands(lua_State* state) noexcept {
    ludork::standard::unregisterEditorCommandBoolControlHandler(
        state, "performanceMonitor");
    ludork::standard::unregisterEditorCommandShutdownHandler(state);
    ludork::standard::unregisterEditorCommandEnvironment(state, "GlobalCore");
}

}  // namespace

void initializeGlobalLifecycle(lua_State* state) {
    PerformanceProfiler::setEnabled(false);
    System::initializeRuntimeSession();
    initializeActorAudioBridge();
    initializeUiAudioBridge();
    registerVideoPlayback(state);
    ludork::standard::registerRuntimeCleanup(state, ludork::global::shutdown);
    registerEditorCommands(state);
}

namespace ludork::global {

void setRuntimeLaunchOptions(const RuntimeLaunchOptions& options) noexcept {
    launchOptions = options;
}

const RuntimeLaunchOptions& runtimeLaunchOptions() noexcept {
    return launchOptions;
}

void shutdown(lua_State* state) noexcept {
    if (state != nullptr) {
        unregisterEditorCommands(state);
    }
    shutdownVideoPlayback();
    System::shutdownRuntime();
    PerformanceProfiler::shutdown();
    UIManager::shutdown();
    WeatherController::shutdown();
    FogController::shutdown();
    shutdownUiAudioBridge();
    AudioManager::shutdown();
    CommonTipController::shutdown();
    FontManager::clear();
    ShaderManager::clear();
    TextureManager::clear();
    TimeManager::shutdown();
    shutdownActorAudioBridge();
    SystemConfigBase::shutdown();
}

}  // namespace ludork::global
