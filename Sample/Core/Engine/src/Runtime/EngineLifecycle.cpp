#include <EngineLifecycle.hpp>

#include <Animation.hpp>
#include <EditorCommandServices.hpp>
#include <FileBatchJson.hpp>
#include <Gameplay/Actor.hpp>
#include <Input/InputService.hpp>
#include <LudorkCoreBinding/ValueCodec.hpp>
#include <NodeGraph/LatentManager.hpp>
#include <Runtime/EngineRuntimeServices.hpp>
#include <Runtime/EngineState.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <RuntimeSession.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/Rect.hpp>
#include <UI/TextEffectResources.hpp>
#include <UI/UiControlAdapterRegistry.hpp>
#include <UI/UiResources.hpp>
#include <UI/UiVector4CurveResource.hpp>
#include <Utils/EventBus.hpp>
#include <Utils/File.hpp>
#include <Utils/Inner.hpp>

#include "FileBatchJsonConversion.hpp"

#include <memory>

extern "C" {
#include <lua.h>
}

namespace {

void registerEditorCommands(lua_State* state) {
    const int stackBase = lua_gettop(state);
    lua_getglobal(state, "Engine");
    if (lua_istable(state, -1)) {
        ludork::standard::registerEditorCommandEnvironment(state, "Engine", -1);
        lua_getfield(state, -1, "Input");
        if (lua_istable(state, -1)) {
            lua_getfield(state, -1, "injectEvent");
            if (lua_isfunction(state, -1)) {
                ludork::standard::registerEditorCommandInputHandler(state, -1);
            }
        }
    }
    lua_settop(state, stackBase);
}

void unregisterEditorCommands(lua_State* state) noexcept {
    ludork::standard::unregisterEditorCommandInputHandler(state);
    ludork::standard::unregisterEditorCommandEnvironment(state, "Engine");
}

}  // namespace

void initializeEngineLifecycle(lua_State* state) {
    ludork::standard::configureFileBatchJson(
        state,
        [](const std::string& content) {
            return std::make_shared<const RuntimeValue>(parseJSONText(content));
        },
        ludork::engine::beginFileBatchJsonConversion,
        ludork::engine::stepFileBatchJsonConversion,
        ludork::engine::clearFileBatchJsonConversion);
    ludork::standard::registerRuntimeCleanup(state, ludork::engine::shutdown);
    registerEditorCommands(state);
}

namespace ludork::engine {

void shutdown(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active()) {
        return;
    }
    ludork::standard::clearFileBatchJson(state);
    unregisterEditorCommands(state);
    GameRunning = false;
    shutdownEngineRuntimeServices(state);
    shutdownLatent();
    inputService().shutdown();
    shutdownEventBus();
    clearRuntimeResolver();
    FunctionalBase::resetRuntimeCallbacks();
    Rect::clearOpacityCurveCache();
    ludork::engine::text_effects::clearResources();
    clearUiControlAdapterResourceCache();
    clearUiVector4CurveResourceCache();
    uiResources().reset();
    setActorAudioService(nullptr);
    shutdownAnimationResources();
    shutdownActorResources();
    shutdownInner();
    resetEngineState();
}

}  // namespace ludork::engine
