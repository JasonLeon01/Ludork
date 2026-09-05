#pragma once

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <string>
#include <vector>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

struct CallableInfo {
    int parameterCount = 0;
    bool vararg = false;
    std::vector<std::string> parameterNames;
};

CallableInfo inspectCallable(const sol::object& callable);
sol::table constructorClass(lua_State* state);
void setClassClosure(lua_State* state, const sol::table& target,
                     const char* name, const sol::table& classTable,
                     lua_CFunction function);
sol::table finalizeClassImpl(sol::table definition, const sol::table& bases);
sol::table ownFields(sol::state_view lua, const sol::object& target);
sol::object rawOwnField(sol::state_view lua, const sol::object& target,
                        const sol::object& key);
bool hasRawOwnField(sol::state_view lua, const sol::object& target,
                    const sol::object& key);
sol::table ownKeyList(sol::state_view lua, const sol::object& target);
sol::table mroCopy(sol::state_view lua, const sol::object& value);
int classNew(lua_State* state);
int classCall(lua_State* state);

}  // namespace ludork::standard::class_runtime::detail
