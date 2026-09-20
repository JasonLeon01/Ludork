#include "Native/NativeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/TypedFields.hpp"

#include <LuaGlue/LuaGlue.hpp>

#include <cstddef>

namespace ludork::standard::class_runtime::detail {

lua_glue::Object rawMember(lua_glue::StateView lua, const lua_glue::Table& type,
                           const lua_glue::Object& key) {
    lua_glue::Object result = type.raw_get<lua_glue::Object>(key);
    if ((!result.valid() || result.get_type() == lua_glue::Type::Nil) &&
        !isClass(type)) {
        if (nativeClassProperty(lua, type, key, result)) {
            return result;
        }
        const lua_glue::Object rawIndex =
            class_native::getObjectMetatable(lua,
                                             lua_glue::MakeObject(lua, type))
                .raw_get<lua_glue::Object>("__index");
        if (rawIndex.is<lua_glue::Function>()) {
            lua_glue::CallResult indexed =
                rawIndex.as<lua_glue::Function>()(type, key);
            if (indexed.valid()) {
                result = indexed.get<lua_glue::Object>();
            }
        }
    }
    return result.valid() ? result : nilObject(lua);
}

lua_glue::Object findInClass(lua_glue::StateView lua,
                             const lua_glue::Table& classTable,
                             const lua_glue::Object& key, bool includeClass) {
    lua_glue::Table owners = classLookupOwners(
        lua, classTable, includeClass ? "members" : "baseMembers");
    const lua_glue::Object rawOwner = owners.raw_get<lua_glue::Object>(key);
    if (rawOwner.is<lua_glue::Table>()) {
        const lua_glue::Object cached =
            rawMember(lua, rawOwner.as<lua_glue::Table>(), key);
        if ((cached.valid() && cached.get_type() != lua_glue::Type::Nil) ||
            hasExplicitNilField(lua, rawOwner, key)) {
            return cached;
        }
        owners.raw_set(key, lua_glue::nil);
    }
    const lua_glue::Table mro = getMro(lua, classTable);
    const std::size_t start = includeClass ? 1 : 2;
    for (std::size_t index = start; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro[index];
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Object result =
            rawMember(lua, rawType.as<lua_glue::Table>(), key);
        if ((result.valid() && result.get_type() != lua_glue::Type::Nil) ||
            hasExplicitNilField(lua, rawType, key)) {
            owners.raw_set(key, rawType);
            return result;
        }
    }
    return nilObject(lua);
}

}  // namespace ludork::standard::class_runtime::detail
