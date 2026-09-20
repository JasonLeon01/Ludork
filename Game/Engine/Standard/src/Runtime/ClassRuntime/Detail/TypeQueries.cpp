#include "Detail/TypeQueries.hpp"
#include "RuntimeState.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

namespace ludork::standard::class_runtime::detail {

bool isNativeType(lua_glue::StateView lua, const lua_glue::Table& value) {
    return !isClass(value) && typeInfoOf(lua, value).is<lua_glue::Table>();
}

bool isCompositeInstance(lua_glue::StateView lua,
                         const lua_glue::Object& instance) {
    if ((instance.get_type() != lua_glue::Type::Userdata)) {
        return false;
    }
    const lua_glue::Object marker =
        class_native::getObjectMetatable(lua, instance)
            .raw_get<lua_glue::Object>(protocol::COMPOSITE_MARKER_FIELD);
    return marker.is<bool>() && marker.as<bool>();
}

lua_glue::Object scriptClassOf(lua_glue::StateView lua,
                               const lua_glue::Object& value) {
    if (value.get_type() == lua_glue::Type::Table) {
        const lua_glue::Table tableValue = value.as<lua_glue::Table>();
        const lua_glue::Object rawClass =
            tableValue.raw_get<lua_glue::Object>(CLASS_FIELD);
        if (rawClass.is<lua_glue::Table>()) {
            return rawClass;
        }
        const lua_glue::Table metatable =
            class_native::getObjectMetatable(lua, value);
        if (isClass(metatable)) {
            return lua_glue::MakeObject(lua, metatable);
        }
    } else if (value.get_type() == lua_glue::Type::Userdata) {
        lua_State* state = lua.lua_state();
        value.push(lua.lua_state());
        const int valueIndex = lua_absindex(state, -1);
        if (lua_getiuservalue(state, valueIndex, 1) == LUA_TTABLE) {
            lua_getfield(state, -1, CLASS_FIELD);
            const lua_glue::Object rawClass =
                lua_glue::Read<lua_glue::Object>(state, -1);
            lua_pop(state, 3);
            if (rawClass.is<lua_glue::Table>()) {
                return rawClass;
            }
        } else {
            lua_pop(state, 2);
        }
    }
    return nilObject(lua);
}

lua_glue::Object typeInfoOf(lua_glue::StateView lua,
                            const lua_glue::Table& nativeType) {
    return class_native::getObjectMetatable(
               lua, lua_glue::MakeObject(lua, nativeType))
        .raw_get<lua_glue::Object>(protocol::CLASS_TYPE_FIELD);
}

lua_glue::Object nativeTypeOf(lua_glue::StateView lua,
                              const lua_glue::Object& value) {
    if (value.get_type() != lua_glue::Type::Userdata) {
        return nilObject(lua);
    }
    auto pushed = lua_glue::PushGuard(value);
    return lua_glue::NativeTypeTable(lua.lua_state(), pushed.index());
}

lua_glue::Object actualClassOf(lua_glue::StateView lua,
                               const lua_glue::Object& value) {
    if (value.is<lua_glue::Table>() && isClass(value.as<lua_glue::Table>())) {
        return value;
    }
    lua_glue::Object result = scriptClassOf(lua, value);
    if (result.is<lua_glue::Table>()) {
        return result;
    }
    return nativeTypeOf(lua, value);
}

}  // namespace ludork::standard::class_runtime::detail
