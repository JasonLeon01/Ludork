#include "Class/ClassRuntimeInternals.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/TypeQueries.hpp"
#include "Detail/TypedFields.hpp"
#include "Native/NativeRuntime.hpp"

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>

namespace ludork::standard::class_runtime::detail {

lua_glue::Table ownFields(lua_glue::StateView lua,
                          const lua_glue::Object& target) {
    if (target.is<lua_glue::Table>()) {
        return target.as<lua_glue::Table>();
    }
    if (target.get_type() == lua_glue::Type::Userdata) {
        return class_native::getUserFields(lua, target, false);
    }
    return lua.create_table();
}

lua_glue::Object rawOwnField(lua_glue::StateView lua,
                             const lua_glue::Object& target,
                             const lua_glue::Object& key) {
    if (target.get_type() == lua_glue::Type::Userdata) {
        lua_State* state = lua.lua_state();
        target.push(lua.lua_state());
        if (lua_getiuservalue(state, -1, 1) != LUA_TTABLE) {
            lua_pop(state, 2);
            return nilObject(lua);
        }
        key.push(lua.lua_state());
        lua_rawget(state, -2);
        lua_glue::Object result = lua_glue::Read<lua_glue::Object>(state, -1);
        lua_pop(state, 3);
        return result;
    }
    if (target.is<lua_glue::Table>()) {
        return target.as<lua_glue::Table>().raw_get<lua_glue::Object>(key);
    }
    return nilObject(lua);
}

bool hasRawOwnField(lua_glue::StateView lua, const lua_glue::Object& target,
                    const lua_glue::Object& key) {
    const lua_glue::Object value = rawOwnField(lua, target, key);
    const bool explicitNil = hasExplicitNilField(lua, target, key);
    return explicitNil ||
           (value.valid() && value.get_type() != lua_glue::Type::Nil);
}

lua_glue::Table ownKeyList(lua_glue::StateView lua,
                           const lua_glue::Object& target) {
    lua_glue::Table result = lua.create_table();
    for (const auto& entry : ownFields(lua, target)) {
        result.add(entry.first);
    }
    for (const auto& entry : explicitNilFieldKeys(lua, target)) {
        result.add(entry.second);
    }
    return result;
}

lua_glue::Table mroCopy(lua_glue::StateView lua,
                        const lua_glue::Object& value) {
    lua_glue::Object rawClass = value;
    if (!value.is<lua_glue::Table>() ||
        (!isClass(value.as<lua_glue::Table>()) &&
         !isNativeType(lua, value.as<lua_glue::Table>()))) {
        rawClass = actualClassOf(lua, value);
    }
    if (!rawClass.is<lua_glue::Table>()) {
        throw std::invalid_argument(
            "Class.getMro requires a class or class instance");
    }
    const lua_glue::Table mro = getMro(lua, rawClass.as<lua_glue::Table>());
    lua_glue::Table result = lua.create_table();
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        result.add(mro.raw_get<lua_glue::Object>(index));
    }
    return result;
}

}  // namespace ludork::standard::class_runtime::detail
