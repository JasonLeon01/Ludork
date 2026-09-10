#include "HotReload.hpp"
#include "HotReloadImpl.hpp"

#include <EditorCommandServices.hpp>

namespace ludork::runtime {
namespace {

void reload(lua_State* state) {
    HotReloadImpl(state).run();
}

}  // namespace

void initializeHotReload(lua_State* state) {
    ludork::standard::registerEditorCommandReloadHandler(state, reload);
}

void shutdownHotReload(lua_State* state) noexcept {
    ludork::standard::unregisterEditorCommandReloadHandler(state);
}

}  // namespace ludork::runtime
