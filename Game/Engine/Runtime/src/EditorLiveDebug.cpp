#include <Runtime/EditorLiveDebug.hpp>

#include <EditorCommandServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <Runtime/RuntimeSession.hpp>

#include <stdexcept>

void EditorLiveDebug::install(const RuntimeIdentityPtr& handler) {
    ludork::runtime::RuntimeScope runtime;
    lua_glue::StateView lua(runtime.state());
    const lua_glue::Object function =
        ludork::runtime::binding::writeOpaqueIdentity(lua, handler);
    if (!function.is<lua_glue::Function>()) {
        throw std::invalid_argument(
            "Editor live debug handler must be a function");
    }
    function.push(runtime.state());
    ludork::standard::registerEditorLiveDebugHandler(runtime.state(), -1);
    lua_pop(runtime.state(), 1);
}

void EditorLiveDebug::uninstall() {
    ludork::runtime::RuntimeScope runtime;
    ludork::standard::unregisterEditorLiveDebugHandler(runtime.state());
}
