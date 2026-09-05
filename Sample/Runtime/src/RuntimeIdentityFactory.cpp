#include <Runtime/RuntimeValue.hpp>

#include "Runtime/RuntimeSession.hpp"

#include <LudorkRuntimeBinding/NativeObjectCodec.hpp>

#include <sol2/sol.hpp>

RuntimeIdentityPtr createRuntimeMapIdentity() {
    ludork::runtime::RuntimeScope scope;
    sol::state_view lua = sol::state_view(scope.state());
    return ludork::runtime::binding::readOpaqueIdentity<RuntimeIdentityPtr>(
        sol::make_object(lua, lua.create_table()));
}
