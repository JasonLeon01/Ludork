#include <Runtime/RuntimeValue.hpp>

#include "EngineRuntimeSession.hpp"

#include <LudorkCoreBinding/NativeObjectCodec.hpp>

#include <sol2/sol.hpp>

RuntimeIdentityPtr createRuntimeMapIdentity() {
    ludork::engine::runtime_detail::EngineRuntimeScope scope;
    sol::state_view lua = scope.lua();
    return ludork_core::readOpaqueIdentity<RuntimeIdentityPtr>(
        sol::make_object(lua, lua.create_table()));
}
