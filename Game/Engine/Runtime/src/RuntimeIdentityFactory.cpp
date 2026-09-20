#include <Runtime/RuntimeValue.hpp>

#include "Runtime/RuntimeSession.hpp"

#include <LudorkRuntimeBinding/NativeObjectCodec.hpp>

#include <LuaGlue/LuaGlue.hpp>

RuntimeIdentityPtr createRuntimeMapIdentity() {
    ludork::runtime::RuntimeScope scope;
    lua_glue::StateView lua = lua_glue::StateView(scope.state());
    return ludork::runtime::binding::readOpaqueIdentity<RuntimeIdentityPtr>(
        lua_glue::MakeObject(lua, lua.create_table()));
}
