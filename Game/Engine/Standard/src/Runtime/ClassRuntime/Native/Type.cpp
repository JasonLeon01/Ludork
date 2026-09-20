#include "Native/NativeRuntime.hpp"
#include <ClassRuntimeProtocol.hpp>

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"

#include <ClassServices.hpp>
#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::standard::class_runtime::detail {

bool isNativeInitializer(const lua_glue::Table& nativeType,
                         const lua_glue::Object& member) {
    const lua_glue::Object initializer =
        nativeType.raw_get<lua_glue::Object>(NATIVE_INITIALIZER_FIELD);
    return initializer.is<lua_glue::Function>() &&
           member.is<lua_glue::Function>() &&
           objectsRawEqual(initializer, member);
}

std::string nativeTypeName(lua_glue::StateView lua,
                           const lua_glue::Table& nativeType) {
    const lua_glue::Object rawTypeInfo = typeInfoOf(lua, nativeType);
    if (!rawTypeInfo.is<lua_glue::Table>()) {
        throw std::invalid_argument(
            "Native class is missing binding type information");
    }
    const lua_glue::Object rawName =
        rawTypeInfo.as<lua_glue::Table>().raw_get<lua_glue::Object>("name");
    if (!rawName.is<std::string>()) {
        throw std::invalid_argument(
            "Native class is missing its qualified type name");
    }
    return rawName.as<std::string>();
}

lua_glue::Object nativeTypeDefinition(lua_glue::StateView lua,
                                      const lua_glue::Table& nativeType,
                                      const lua_glue::Object& key) {
    const lua_glue::Object member = nativeType.raw_get<lua_glue::Object>(key);
    if (member.valid() && member.get_type() != lua_glue::Type::Nil) {
        return member;
    }
    const lua_glue::Object getters =
        nativeType.raw_get<lua_glue::Object>(protocol::CLASS_GETTERS_FIELD);
    if (getters.is<lua_glue::Table>()) {
        return getters.as<lua_glue::Table>().raw_get<lua_glue::Object>(key);
    }
    return nilObject(lua);
}

lua_glue::Object nativeClassDefaultResolverKey(lua_glue::StateView lua) {
    return lua_glue::MakeObject(
        lua, lua_glue::LightUserdata(
                 static_cast<void*>(&nativeClassDefaultResolverKeyStorage)));
}

namespace {

// The protected call owns all allocating Lua operations. Only the final
// boolean crosses back to C++; intermediate cache tables stay on this stack.
int nativePropertyLookup(lua_State* state) {
    luaL_checktype(state, 1, LUA_TTABLE);
    lua_pushstring(state, NATIVE_PROPERTIES_FIELD);
    lua_rawget(state, 1);
    const int properties = lua_absindex(state, -1);
    if (!lua_istable(state, properties) || lua_rawlen(state, properties) == 0) {
        lua_pushboolean(state, false);
        return 1;
    }
    const std::size_t count = lua_rawlen(state, properties);
    lua_pushstring(state, NATIVE_PROPERTY_CACHE_KEY);
    lua_rawget(state, LUA_REGISTRYINDEX);
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_createtable(state, 0, 1);
        lua_pushliteral(state, "k");
        lua_setfield(state, -2, "__mode");
        lua_setmetatable(state, -2);
        lua_pushstring(state, NATIVE_PROPERTY_CACHE_KEY);
        lua_pushvalue(state, -2);
        lua_rawset(state, LUA_REGISTRYINDEX);
    }
    const int cache = lua_absindex(state, -1);
    lua_pushvalue(state, 1);
    lua_rawget(state, cache);
    const int entry = lua_absindex(state, -1);
    if (lua_istable(state, entry)) {
        lua_pushliteral(state, "source");
        lua_rawget(state, entry);
        const bool sameSource = lua_rawequal(state, -1, properties) != 0;
        lua_pop(state, 1);
        lua_pushliteral(state, "count");
        lua_rawget(state, entry);
        const bool sameCount =
            lua_isinteger(state, -1) && lua_tointeger(state, -1) >= 0 &&
            static_cast<std::size_t>(lua_tointeger(state, -1)) == count;
        lua_pop(state, 1);
        lua_pushliteral(state, "members");
        lua_rawget(state, entry);
        if (sameSource && sameCount && lua_istable(state, -1)) {
            lua_pushvalue(state, 2);
            lua_rawget(state, -2);
            lua_pushboolean(
                state, lua_isboolean(state, -1) && lua_toboolean(state, -1));
            return 1;
        }
        lua_pop(state, 1);
    }
    lua_newtable(state);
    const int members = lua_absindex(state, -1);
    for (std::size_t index = 1; index <= count; ++index) {
        lua_rawgeti(state, properties, static_cast<lua_Integer>(index));
        if (lua_type(state, -1) == LUA_TSTRING) {
            lua_pushboolean(state, true);
            lua_rawset(state, members);
        } else {
            lua_pop(state, 1);
        }
    }
    lua_createtable(state, 0, 3);
    lua_pushvalue(state, properties);
    lua_setfield(state, -2, "source");
    lua_pushinteger(state, static_cast<lua_Integer>(count));
    lua_setfield(state, -2, "count");
    lua_pushvalue(state, members);
    lua_setfield(state, -2, "members");
    lua_pushvalue(state, 1);
    lua_pushvalue(state, -2);
    lua_rawset(state, cache);
    lua_pushvalue(state, 2);
    lua_rawget(state, members);
    lua_pushboolean(state,
                    lua_isboolean(state, -1) && lua_toboolean(state, -1));
    return 1;
}

lua_glue::Object resolveNativeClassDefault(lua_glue::StateView lua,
                                           lua_glue::Table nativeType,
                                           const lua_glue::Object& key,
                                           lua_glue::Table defaults,
                                           const lua_glue::Object& value) {
    const lua_glue::Object rawResolvedDefaults =
        nativeType.raw_get<lua_glue::Object>(
            NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD);
    lua_glue::Table resolvedDefaults =
        rawResolvedDefaults.is<lua_glue::Table>()
            ? rawResolvedDefaults.as<lua_glue::Table>()
            : lua.create_table();
    if (!rawResolvedDefaults.is<lua_glue::Table>()) {
        nativeType.raw_set(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD,
                           resolvedDefaults);
    }
    const lua_glue::Object rawResolved =
        resolvedDefaults.raw_get<lua_glue::Object>(key);
    if (rawResolved.is<bool>() && rawResolved.as<bool>()) {
        return value;
    }
    const lua_glue::Object rawMetadata =
        nativeType.raw_get<lua_glue::Object>(protocol::RUNTIME_METADATA_FIELD);
    if (!rawMetadata.is<lua_glue::Table>()) {
        return value;
    }
    const lua_glue::Object rawFieldMetadata =
        rawMetadata.as<lua_glue::Table>().raw_get<lua_glue::Object>(key);
    if (!rawFieldMetadata.is<lua_glue::Table>()) {
        return value;
    }
    const lua_glue::Object rawResolver =
        lua.registry().raw_get<lua_glue::Object>(
            nativeClassDefaultResolverKey(lua));
    if (!rawResolver.is<lua_glue::Function>()) {
        return value;
    }
    lua_glue::Function resolver = rawResolver.as<lua_glue::Function>();
    const lua_glue::Object rawModule =
        rawMetadata.as<lua_glue::Table>().raw_get<lua_glue::Object>("module");
    lua_glue::CallResult result =
        resolver(value, rawFieldMetadata.as<lua_glue::Table>(), rawModule);
    if (!result.valid()) {
        const std::string error = result.error();
        throw std::runtime_error(error.c_str());
    }
    if (result.return_count() == 0) {
        throw std::runtime_error(
            "Native class default resolver returned no value");
    }
    const lua_glue::Object resolved = result.get<lua_glue::Object>();
    defaults.raw_set(key, resolved);
    resolvedDefaults.raw_set(key, true);
    return resolved;
}

lua_glue::Object nativeClassIndex(lua_glue::Table nativeType,
                                  lua_glue::Object key,
                                  lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    lua_glue::Object value = nilObject(lua);
    if (nativeClassProperty(lua, nativeType, key, value)) {
        return value;
    }
    const lua_glue::Table metatable = class_native::getObjectMetatable(
        lua, lua_glue::MakeObject(lua, nativeType));
    const lua_glue::Object original =
        metatable.raw_get<lua_glue::Object>(NATIVE_CLASS_INDEX_FIELD);
    if (original.is<lua_glue::Function>()) {
        lua_glue::CallResult result =
            original.as<lua_glue::Function>()(nativeType, key);
        if (!result.valid()) {
            const std::string error = result.error();
            throw std::runtime_error(error.c_str());
        }
        return result.return_count() == 0 ? nilObject(lua)
                                          : result.get<lua_glue::Object>();
    }
    if (original.is<lua_glue::Table>()) {
        return protectedIndex(lua, original, key);
    }
    return nilObject(lua);
}

void nativeClassNewIndex(lua_glue::Table nativeType, lua_glue::Object key,
                         lua_glue::Object value, lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    lua_glue::Object ignored = nilObject(lua);
    if (nativeClassProperty(lua, nativeType, key, ignored)) {
        nativeType.raw_set(key, value);
        invalidateClassLookup(lua, nativeType);
        return;
    }
    const lua_glue::Table metatable = class_native::getObjectMetatable(
        lua, lua_glue::MakeObject(lua, nativeType));
    const lua_glue::Object original =
        metatable.raw_get<lua_glue::Object>(NATIVE_CLASS_NEW_INDEX_FIELD);
    if (original.is<lua_glue::Function>()) {
        lua_glue::CallResult result =
            original.as<lua_glue::Function>()(nativeType, key, value);
        if (!result.valid()) {
            const std::string error = result.error();
            throw std::runtime_error(error.c_str());
        }
        return;
    }
    if (original.is<lua_glue::Table>()) {
        original.as<lua_glue::Table>().raw_set(key, value);
        return;
    }
    nativeType.raw_set(key, value);
}

}  // namespace

bool nativeTypeDeclaresProperty(const lua_glue::Table& nativeType,
                                const lua_glue::Object& key) {
    if (key.get_type() != lua_glue::Type::String) {
        return false;
    }
    lua_State* state = nativeType.lua_state();
    lua_glue::detail::AccessScope execution(state);
    if (!execution.active()) {
        throw std::runtime_error("Lua state is unavailable or stopping");
    }
    lua_glue::StackGuard stack(state);
    if (!lua_checkstack(state, 3)) {
        throw std::runtime_error("Native property lookup stack cannot grow");
    }
    lua_pushcfunction(state, nativePropertyLookup);
    nativeType.push(state);
    key.push(state);
    lua_glue::ProtectedStackCall(state, 2, 1);
    return lua_toboolean(state, -1) != 0;
}

bool nativeClassProperty(lua_glue::StateView lua,
                         const lua_glue::Table& nativeType,
                         const lua_glue::Object& key, lua_glue::Object& value) {
    const lua_glue::Table mro = getMro(lua, nativeType);
    lua_glue::Object rawOverride = nilObject(lua);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro.raw_get<lua_glue::Object>(index);
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table current = rawType.as<lua_glue::Table>();
        if (rawOverride.get_type() == lua_glue::Type::Nil) {
            const lua_glue::Object rawValue =
                current.raw_get<lua_glue::Object>(key);
            if (rawValue.valid() &&
                rawValue.get_type() != lua_glue::Type::Nil) {
                rawOverride = rawValue;
            }
        }
        if (!nativeTypeDeclaresProperty(current, key)) {
            continue;
        }
        if (rawOverride.get_type() != lua_glue::Type::Nil) {
            value = rawOverride;
            return true;
        }
        const lua_glue::Object rawDefaults =
            current.raw_get<lua_glue::Object>(CLASS_DEFAULTS_FIELD);
        if (rawDefaults.is<lua_glue::Table>()) {
            lua_glue::Table defaults = rawDefaults.as<lua_glue::Table>();
            value = defaults.raw_get<lua_glue::Object>(key);
            if (value.valid() && value.get_type() != lua_glue::Type::Nil) {
                value = resolveNativeClassDefault(lua, current, key, defaults,
                                                  value);
            }
        } else {
            value = nilObject(lua);
        }
        if (!value.valid()) {
            value = nilObject(lua);
        }
        return true;
    }
    return false;
}

bool nativeFallbackMemberEligible(const lua_glue::Object& key) {
    if (!key.is<std::string>()) {
        return true;
    }
    const std::string name = key.as<std::string>();
    return !name.starts_with("__") && name != "class_check" &&
           name != "class_cast";
}

std::vector<lua_glue::Table> nativeRoots(lua_glue::StateView lua,
                                         const lua_glue::Table& classTable) {
    std::vector<lua_glue::Table> result;
    const lua_glue::Table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro[index];
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table nativeType = rawType.as<lua_glue::Table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        bool covered = false;
        for (const lua_glue::Table& root : result) {
            if (derivesFrom(lua, root, nativeType)) {
                covered = true;
                break;
            }
        }
        if (!covered) {
            result.push_back(nativeType);
        }
    }
    return result;
}

}  // namespace ludork::standard::class_runtime::detail

namespace ludork::standard::class_runtime {

void registerNativeClass(lua_glue::Table nativeType,
                         const lua_glue::Table& metadata) {
    using namespace ludork::standard::class_runtime::detail;
    lua_glue::StateView lua(nativeType.lua_state());
    registryTable(lua, NATIVE_PROPERTY_CACHE_KEY, "k")
        .raw_set(nativeType, lua_glue::nil);
    lua_glue::Table defaults = lua.create_table();
    const lua_glue::Object rawAttrs =
        metadata.raw_get<lua_glue::Object>("attrs");
    if (rawAttrs.is<lua_glue::Table>()) {
        const lua_glue::Table attrs = rawAttrs.as<lua_glue::Table>();
        for (std::size_t index = 1; index <= attrs.size(); ++index) {
            const lua_glue::Object rawName =
                attrs.raw_get<lua_glue::Object>(index);
            if (!rawName.is<std::string>()) {
                continue;
            }
            const lua_glue::Object rawField =
                metadata.raw_get<lua_glue::Object>(rawName.as<std::string>());
            if (!rawField.is<lua_glue::Table>()) {
                continue;
            }
            const lua_glue::Object rawDefault =
                rawField.as<lua_glue::Table>().raw_get<lua_glue::Object>(
                    "default");
            if (!rawDefault.valid() ||
                rawDefault.get_type() == lua_glue::Type::Nil) {
                continue;
            }
            const lua_glue::Object value =
                class_runtime::clonePlainData(lua, rawDefault);
            defaults.raw_set(rawName, value);
        }
    }
    nativeType.raw_set(CLASS_DEFAULTS_FIELD, defaults);
    nativeType.raw_set(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD,
                       lua.create_table());

    lua_glue::Table metatable = class_native::getObjectMetatable(
        lua, lua_glue::MakeObject(lua, nativeType));
    const lua_glue::Object rawGuard =
        metatable.raw_get<lua_glue::Object>(NATIVE_CLASS_GUARD_FIELD);
    if (rawGuard.is<bool>() && rawGuard.as<bool>()) {
        return;
    }
    metatable.raw_set(NATIVE_CLASS_INDEX_FIELD,
                      metatable.raw_get<lua_glue::Object>("__index"));
    metatable.raw_set(NATIVE_CLASS_NEW_INDEX_FIELD,
                      metatable.raw_get<lua_glue::Object>("__newindex"));
    metatable.set_function("__index", &nativeClassIndex);
    metatable.set_function("__newindex", &nativeClassNewIndex);
    metatable.raw_set(NATIVE_CLASS_GUARD_FIELD, true);
}

void registerNativeClassDefaultResolver(lua_glue::StateView lua,
                                        const lua_glue::Function& callback) {
    lua.registry().raw_set(detail::nativeClassDefaultResolverKey(lua),
                           callback);
}

void unregisterNativeClassDefaultResolver(lua_glue::StateView lua) {
    lua.registry().raw_set(detail::nativeClassDefaultResolverKey(lua),
                           lua_glue::nil);
}

}  // namespace ludork::standard::class_runtime
