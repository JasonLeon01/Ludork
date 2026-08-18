#include "Bindings.hpp"

#include "Runtime/ClassRuntime/ClassRuntime.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

namespace {

extern "C" int luaopen_Class(lua_State* state) {
    if (state == nullptr) {
        return 0;
    }
    ludork::standard::class_runtime::createModule(sol::state_view(state))
        .push();
    return 1;
}

}  // namespace

namespace ludork::standard::binding {

void registerClass(sol::state_view lua) {
    static_cast<void>(lua.require("Class", luaopen_Class, true));
}

}  // namespace ludork::standard::binding
