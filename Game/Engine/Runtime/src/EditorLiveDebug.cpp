#include <Runtime/EditorLiveDebug.hpp>

#include <EditorCommandServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <Runtime/RuntimeSession.hpp>

#include <stdexcept>

void EditorLiveDebug::install(const RuntimeIdentityPtr& handler) {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua(runtime.state());
    const sol::object function =
        ludork::runtime::binding::writeOpaqueIdentity(lua, handler);
    if (!function.is<sol::protected_function>()) {
        throw std::invalid_argument(
            "Editor live debug handler must be a function");
    }
    function.push();
    ludork::standard::registerEditorLiveDebugHandler(runtime.state(), -1);
    lua_pop(runtime.state(), 1);
}

void EditorLiveDebug::uninstall() {
    ludork::runtime::RuntimeScope runtime;
    ludork::standard::unregisterEditorLiveDebugHandler(runtime.state());
}
