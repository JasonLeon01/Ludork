#include "Internal.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

bool nativeTypeAccepts(sol::state_view lua, const sol::table& nativeType,
                       const sol::object& value) {
    const sol::object rawTypeInfo = typeInfoOf(lua, nativeType);
    if (!rawTypeInfo.is<sol::table>()) {
        return false;
    }
    const sol::object rawIs =
        rawTypeInfo.as<sol::table>().raw_get<sol::object>("is");
    if (!rawIs.is<sol::protected_function>()) {
        return false;
    }
    sol::protected_function_result result =
        rawIs.as<sol::protected_function>()(value);
    return result.valid() && result.get_type() == sol::type::boolean &&
           result.get<bool>();
}

void registerMethodOwner(sol::state_view lua, const sol::table& classTable,
                         const sol::object& value) {
    if (!value.is<sol::function>()) {
        return;
    }
    registryTable(lua, METHOD_OWNERS_KEY, "k").raw_set(value, classTable);
}

namespace {

void* nativePointer(lua_State* state, int index) {
    const int absoluteIndex = lua_absindex(state, index);
    if (lua_type(state, absoluteIndex) != LUA_TUSERDATA ||
        lua_getmetatable(state, absoluteIndex) == 0) {
        return nullptr;
    }
    lua_getfield(state, -1, "__LuaSFNativeComposite");
    const bool composite = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    lua_getfield(state, -1, "__type");
    const bool native = lua_istable(state, -1);
    lua_pop(state, 2);
    if (composite || !native) {
        return nullptr;
    }
    void* memory = lua_touserdata(state, absoluteIndex);
    if (memory == nullptr) {
        return nullptr;
    }
    void* rawData = sol::detail::align_usertype_pointer(memory);
    return *static_cast<void**>(rawData);
}

}  // namespace

void registerNativePointerOwner(sol::state_view lua,
                                const sol::object& nativeObject,
                                const sol::object& owner) {
    lua_State* state = lua.lua_state();
    nativeObject.push();
    void* pointer = nativePointer(state, -1);
    lua_pop(state, 1);
    if (pointer == nullptr) {
        return;
    }
    registryTable(lua, NATIVE_POINTER_OWNERS_KEY, "v").push();
    lua_pushlightuserdata(state, pointer);
    owner.push();
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

void unregisterNativePointerOwner(sol::state_view lua,
                                  const sol::object& nativeObject,
                                  const sol::object& owner) {
    lua_State* state = lua.lua_state();
    nativeObject.push();
    void* pointer = nativePointer(state, -1);
    lua_pop(state, 1);
    if (pointer == nullptr) {
        return;
    }
    sol::table owners = registryTable(lua, NATIVE_POINTER_OWNERS_KEY, "v");
    owners.push();
    const int ownersIndex = lua_absindex(state, -1);
    lua_pushlightuserdata(state, pointer);
    lua_rawget(state, ownersIndex);
    owner.push();
    const bool matches = lua_rawequal(state, -1, -2) != 0;
    lua_pop(state, 2);
    if (!matches) {
        lua_pop(state, 1);
        return;
    }
    lua_pushlightuserdata(state, pointer);
    lua_pushnil(state);
    lua_rawset(state, ownersIndex);
    lua_pop(state, 1);
}

bool pushNativeOwner(lua_State* state, int nativeIndex) {
    const int absoluteNativeIndex = lua_absindex(state, nativeIndex);
    sol::state_view lua(state);
    registryTable(lua, NATIVE_OWNERS_KEY, "kv").push();
    const int ownersIndex = lua_absindex(state, -1);
    lua_pushvalue(state, absoluteNativeIndex);
    lua_rawget(state, ownersIndex);
    if (!lua_isnil(state, -1)) {
        lua_remove(state, ownersIndex);
        return true;
    }
    lua_pop(state, 2);
    void* pointer = nativePointer(state, absoluteNativeIndex);
    if (pointer == nullptr) {
        return false;
    }
    registryTable(lua, NATIVE_POINTER_OWNERS_KEY, "v").push();
    const int pointerOwnersIndex = lua_absindex(state, -1);
    lua_pushlightuserdata(state, pointer);
    lua_rawget(state, pointerOwnersIndex);
    if (!lua_isnil(state, -1)) {
        lua_remove(state, pointerOwnersIndex);
        return true;
    }
    lua_pop(state, 2);
    return false;
}

void restoreNativeOwners(lua_State* state) {
    const int resultCount = lua_gettop(state);
    for (int index = 1; index <= resultCount; ++index) {
        const int absoluteIndex = lua_absindex(state, index);
        if (lua_type(state, absoluteIndex) == LUA_TUSERDATA) {
            if (pushNativeOwner(state, absoluteIndex)) {
                lua_replace(state, absoluteIndex);
            }
        } else if (lua_type(state, absoluteIndex) == LUA_TTABLE) {
            const lua_Integer length =
                static_cast<lua_Integer>(lua_rawlen(state, absoluteIndex));
            for (lua_Integer itemIndex = 1; itemIndex <= length; ++itemIndex) {
                lua_rawgeti(state, absoluteIndex, itemIndex);
                const int valueIndex = lua_absindex(state, -1);
                if (lua_type(state, valueIndex) == LUA_TUSERDATA &&
                    pushNativeOwner(state, valueIndex)) {
                    lua_replace(state, valueIndex);
                }
                lua_rawseti(state, absoluteIndex, itemIndex);
            }
        }
    }
}

namespace {

int boundMethodCall(lua_State* state) {
    const int argumentCount = lua_gettop(state);
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_pushvalue(state, lua_upvalueindex(2));
    lua_insert(state, 2);
    lua_call(state, argumentCount + 1, LUA_MULTRET);
    restoreNativeOwners(state);
    return lua_gettop(state);
}

int nativeMethodCall(lua_State* state) {
    const int argumentCount = lua_gettop(state);
    if (argumentCount == 0) {
        return luaL_error(state, "Native instance method requires a receiver");
    }
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_pushvalue(state, lua_upvalueindex(2));
    lua_replace(state, 2);
    lua_call(state, argumentCount, LUA_MULTRET);
    restoreNativeOwners(state);
    return lua_gettop(state);
}

}  // namespace

sol::object bindMethod(sol::state_view lua, const sol::object& method,
                       const sol::object& self) {
    lua_State* state = lua.lua_state();
    method.push();
    self.push();
    lua_pushcclosure(state, boundMethodCall, 2);
    sol::object result = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    return result;
}

sol::object wrapNativeMethod(sol::state_view lua, const sol::object& method,
                             const sol::object& nativeObject) {
    method.push();
    nativeObject.push();
    lua_pushcclosure(lua.lua_state(), nativeMethodCall, 2);
    sol::object result = sol::stack::get<sol::object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    return result;
}

bool objectsRawEqual(const sol::object& left, const sol::object& right) {
    lua_State* state = left.lua_state();
    left.push();
    right.push();
    const bool result = lua_rawequal(state, -2, -1) != 0;
    lua_pop(state, 2);
    return result;
}

bool isNativeInitializer(const sol::table& nativeType,
                         const sol::object& member) {
    const sol::object initializer =
        nativeType.raw_get<sol::object>(NATIVE_INITIALIZER_FIELD);
    return initializer.is<sol::function>() && member.is<sol::function>() &&
           objectsRawEqual(initializer, member);
}

namespace {

sol::object cachedBoundMethod(sol::state_view lua, sol::table proxy,
                              const sol::object& key, const sol::object& method,
                              const sol::object& receiver) {
    sol::object rawCache = proxy.raw_get<sol::object>(4);
    sol::table cache = rawCache.is<sol::table>() ? rawCache.as<sol::table>()
                                                 : lua.create_table();
    if (!rawCache.is<sol::table>()) {
        proxy.raw_set(4, cache);
    }
    const sol::object rawEntry = cache.raw_get<sol::object>(key);
    if (rawEntry.is<sol::table>()) {
        const sol::table entry = rawEntry.as<sol::table>();
        const sol::object cachedMethod = entry.raw_get<sol::object>(1);
        const sol::object cachedReceiver = entry.raw_get<sol::object>(2);
        const sol::object cachedWrapper = entry.raw_get<sol::object>(3);
        if (cachedWrapper.is<sol::function>() &&
            objectsRawEqual(cachedMethod, method) &&
            objectsRawEqual(cachedReceiver, receiver)) {
            return cachedWrapper;
        }
    }
    sol::table entry = lua.create_table();
    const sol::object wrapper = bindMethod(lua, method, receiver);
    entry.raw_set(1, method);
    entry.raw_set(2, receiver);
    entry.raw_set(3, wrapper);
    cache.raw_set(key, entry);
    return wrapper;
}

int superProxyIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        const sol::table proxy = sol::stack::get<sol::table>(state, 1);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object self = proxy.raw_get<sol::object>(1);
        const sol::table actualClass = proxy.raw_get<sol::table>(2);
        const std::size_t currentIndex = proxy.raw_get<std::size_t>(3);
        const sol::table mro = getMro(lua, actualClass);
        for (std::size_t index = currentIndex + 1; index <= mro.size();
             ++index) {
            const sol::object rawType = mro.raw_get<sol::object>(index);
            if (!rawType.is<sol::table>()) {
                continue;
            }
            const sol::table type = rawType.as<sol::table>();
            const sol::object rawGetters =
                type.raw_get<sol::object>("__getters");
            if (rawGetters.is<sol::table>()) {
                const sol::object getter =
                    rawGetters.as<sol::table>().raw_get<sol::object>(key);
                if (getter.is<sol::function>()) {
                    getter.push();
                    self.push();
                    lua_call(state, 1, 1);
                    return 1;
                }
            }
            sol::object member = nilObject(lua);
            if (isNativeType(lua, type)) {
                const sol::object rawBaseMethods =
                    type.raw_get<sol::object>("__classBaseMethods");
                if (rawBaseMethods.is<sol::table>()) {
                    member =
                        rawBaseMethods.as<sol::table>().raw_get<sol::object>(
                            key);
                }
            }
            if (!member.valid() || member.get_type() == sol::type::lua_nil) {
                member = rawMember(lua, type, key);
            }
            if (!member.valid() || member.get_type() == sol::type::lua_nil) {
                continue;
            }
            if (!member.is<sol::function>()) {
                member.push();
                return 1;
            }
            sol::object receiver = self;
            if (isNativeType(lua, type) && !isNativeInitializer(type, member)) {
                const sol::table fields =
                    class_native::getUserFields(lua, self, false);
                sol::object nativeObject =
                    nativeObjectForType(lua, fields, type);
                if (!nativeObject.is<sol::userdata>()) {
                    nativeObject = ensureDefaultNativeObject(lua, self, type);
                }
                if (nativeObject.is<sol::userdata>()) {
                    receiver = nativeObject;
                }
            }
            cachedBoundMethod(lua, proxy, key, member, receiver).push();
            return 1;
        }
        lua_pushnil(state);
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

sol::table superProxyMetatable(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object rawMetatable =
        registry.raw_get<sol::object>(SUPER_PROXY_METATABLE_KEY);
    if (rawMetatable.is<sol::table>()) {
        return rawMetatable.as<sol::table>();
    }
    sol::table metatable = lua.create_table();
    metatable.push();
    lua_pushcfunction(lua.lua_state(), superProxyIndex);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pop(lua.lua_state(), 1);
    registry.raw_set(SUPER_PROXY_METATABLE_KEY, metatable);
    return metatable;
}

sol::table createSuperProxy(sol::state_view lua, const sol::table& currentClass,
                            const sol::object& self) {
    const sol::object rawActualClass = actualClassOf(lua, self);
    if (!rawActualClass.is<sol::table>()) {
        throw std::invalid_argument("super() requires a class instance");
    }
    const sol::table actualClass = rawActualClass.as<sol::table>();
    const sol::table mro = getMro(lua, actualClass);
    std::size_t currentIndex = 0;
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (rawType.is<sol::table>() &&
            objectsRawEqual(rawType.as<sol::table>(), currentClass)) {
            currentIndex = index;
            break;
        }
    }
    if (currentIndex == 0) {
        throw std::invalid_argument(
            "super() current class is not in the instance MRO");
    }
    sol::table cache = registryTable(lua, SUPER_PROXY_CACHE_KEY, "k");
    const sol::object rawInstanceCache = cache.raw_get<sol::object>(self);
    sol::table instanceCache = rawInstanceCache.is<sol::table>()
                                   ? rawInstanceCache.as<sol::table>()
                                   : createWeakTable(lua, "v");
    if (!rawInstanceCache.is<sol::table>()) {
        cache.raw_set(self, instanceCache);
    }
    const sol::object rawProxy =
        instanceCache.raw_get<sol::object>(currentClass);
    if (rawProxy.is<sol::table>()) {
        return rawProxy.as<sol::table>();
    }
    sol::table proxy = lua.create_table();
    proxy.raw_set(1, self);
    proxy.raw_set(2, actualClass);
    proxy.raw_set(3, currentIndex);
    proxy[sol::metatable_key] = superProxyMetatable(lua);
    instanceCache.raw_set(currentClass, proxy);
    return proxy;
}

bool inferSuperContext(lua_State* state, sol::table& currentClass,
                       sol::object& self) {
    lua_Debug record{};
    if (lua_getstack(state, 1, &record) == 0 ||
        lua_getinfo(state, "f", &record) == 0) {
        return false;
    }
    sol::state_view lua(state);
    const sol::object caller = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    const sol::object rawOwner =
        registryTable(lua, METHOD_OWNERS_KEY, "k").raw_get<sol::object>(caller);
    if (!rawOwner.is<sol::table>()) {
        return false;
    }
    currentClass = rawOwner.as<sol::table>();
    if (lua_getlocal(state, &record, 1) == nullptr) {
        return false;
    }
    self = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 1);
    return true;
}

}  // namespace

int superFunction(lua_State* state) {
    sol::state_view lua(state);
    const int argumentCount = lua_gettop(state);
    if (argumentCount == 2) {
        if (!lua_istable(state, 1)) {
            return luaL_error(state, "super() first argument must be a class");
        }
        createSuperProxy(lua, sol::stack::get<sol::table>(state, 1),
                         sol::stack::get<sol::object>(state, 2))
            .push();
        return 1;
    }
    if (argumentCount != 0 && argumentCount != 1) {
        return luaL_error(state, "super() expects zero, one, or two arguments");
    }
    sol::table currentClass = lua.create_table();
    sol::object inferredSelf = nilObject(lua);
    if (!inferSuperContext(state, currentClass, inferredSelf)) {
        return luaL_error(state,
                          "super() could not determine the defining class");
    }
    const sol::object self = argumentCount == 1
                                 ? sol::stack::get<sol::object>(state, 1)
                                 : inferredSelf;
    createSuperProxy(lua, currentClass, self).push();
    return 1;
}

bool isNativeType(sol::state_view lua, const sol::table& value) {
    return !isClass(value) && typeInfoOf(lua, value).is<sol::table>();
}

std::string nativeTypeName(sol::state_view lua, const sol::table& nativeType) {
    const sol::object rawTypeInfo = typeInfoOf(lua, nativeType);
    if (!rawTypeInfo.is<sol::table>()) {
        throw std::invalid_argument(
            "Native class is missing binding type information");
    }
    const sol::object rawName =
        rawTypeInfo.as<sol::table>().raw_get<sol::object>("name");
    if (!rawName.is<std::string>()) {
        throw std::invalid_argument(
            "Native class is missing its qualified type name");
    }
    return rawName.as<std::string>();
}

sol::object nativeTypeDefinition(sol::state_view lua,
                                 const sol::table& nativeType,
                                 const sol::object& key) {
    const std::string registryName = "sol." + nativeTypeName(lua, nativeType);
    const sol::object rawMetatable =
        lua.registry().raw_get<sol::object>(registryName);
    if (!rawMetatable.is<sol::table>()) {
        return nilObject(lua);
    }
    return rawMetatable.as<sol::table>().raw_get<sol::object>(key);
}

bool nativeTypeDeclaresProperty(const sol::table& nativeType,
                                const sol::object& key) {
    if (!key.is<std::string>()) {
        return false;
    }
    const sol::object rawProperties =
        nativeType.raw_get<sol::object>("__nativeProperties");
    if (!rawProperties.is<sol::table>()) {
        return false;
    }
    const std::string name = key.as<std::string>();
    const sol::table properties = rawProperties.as<sol::table>();
    for (std::size_t index = 1; index <= properties.size(); ++index) {
        const sol::object rawName = properties.raw_get<sol::object>(index);
        if (rawName.is<std::string>() && rawName.as<std::string>() == name) {
            return true;
        }
    }
    return false;
}

sol::object nativeClassDefaultResolverKey(sol::state_view lua) {
    return sol::make_object(
        lua, sol::lightuserdata_value(
                 static_cast<void*>(&nativeClassDefaultResolverKeyStorage)));
}

namespace {

sol::object resolveNativeClassDefault(sol::state_view lua,
                                      sol::table nativeType,
                                      const sol::object& key,
                                      sol::table defaults,
                                      const sol::object& value) {
    const sol::object rawResolvedDefaults =
        nativeType.raw_get<sol::object>(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD);
    sol::table resolvedDefaults = rawResolvedDefaults.is<sol::table>()
                                      ? rawResolvedDefaults.as<sol::table>()
                                      : lua.create_table();
    if (!rawResolvedDefaults.is<sol::table>()) {
        nativeType.raw_set(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD,
                           resolvedDefaults);
    }
    const sol::object rawResolved = resolvedDefaults.raw_get<sol::object>(key);
    if (rawResolved.is<bool>() && rawResolved.as<bool>()) {
        return value;
    }
    const sol::object rawMetadata =
        nativeType.raw_get<sol::object>("__runtimeMetadata");
    if (!rawMetadata.is<sol::table>()) {
        return value;
    }
    const sol::object rawFieldMetadata =
        rawMetadata.as<sol::table>().raw_get<sol::object>(key);
    if (!rawFieldMetadata.is<sol::table>()) {
        return value;
    }
    const sol::object rawResolver = lua.registry().raw_get<sol::object>(
        nativeClassDefaultResolverKey(lua));
    if (!rawResolver.is<sol::protected_function>()) {
        return value;
    }
    sol::protected_function resolver =
        rawResolver.as<sol::protected_function>();
    const sol::object rawModule =
        rawMetadata.as<sol::table>().raw_get<sol::object>("module");
    sol::protected_function_result result =
        resolver(value, rawFieldMetadata.as<sol::table>(), rawModule);
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    if (result.return_count() == 0) {
        throw std::runtime_error(
            "Native class default resolver returned no value");
    }
    const sol::object resolved = result.get<sol::object>();
    defaults.raw_set(key, resolved);
    resolvedDefaults.raw_set(key, true);
    return resolved;
}

}  // namespace

bool nativeClassProperty(sol::state_view lua, const sol::table& nativeType,
                         const sol::object& key, sol::object& value) {
    const sol::table mro = getMro(lua, nativeType);
    sol::object rawOverride = nilObject(lua);
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const sol::object rawType = mro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table current = rawType.as<sol::table>();
        if (rawOverride.get_type() == sol::type::lua_nil) {
            const sol::object rawValue = current.raw_get<sol::object>(key);
            if (rawValue.valid() && rawValue.get_type() != sol::type::lua_nil) {
                rawOverride = rawValue;
            }
        }
        if (!nativeTypeDeclaresProperty(current, key)) {
            continue;
        }
        if (rawOverride.get_type() != sol::type::lua_nil) {
            value = rawOverride;
            return true;
        }
        const sol::object rawDefaults =
            current.raw_get<sol::object>("__classDefaults");
        if (rawDefaults.is<sol::table>()) {
            sol::table defaults = rawDefaults.as<sol::table>();
            value = defaults.raw_get<sol::object>(key);
            if (value.valid() && value.get_type() != sol::type::lua_nil) {
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

namespace {

sol::object nativeClassIndex(sol::table nativeType, sol::object key,
                             sol::this_state state) {
    sol::state_view lua(state);
    sol::object value = nilObject(lua);
    if (nativeClassProperty(lua, nativeType, key, value)) {
        return value;
    }
    const sol::table metatable =
        class_native::getObjectMetatable(lua, sol::make_object(lua, nativeType));
    const sol::object original =
        metatable.raw_get<sol::object>(NATIVE_CLASS_INDEX_FIELD);
    if (original.is<sol::protected_function>()) {
        sol::protected_function_result result =
            original.as<sol::protected_function>()(nativeType, key);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return result.return_count() == 0 ? nilObject(lua)
                                          : result.get<sol::object>();
    }
    if (original.is<sol::table>()) {
        return protectedIndex(lua, original, key);
    }
    return nilObject(lua);
}

void nativeClassNewIndex(sol::table nativeType, sol::object key,
                         sol::object value, sol::this_state state) {
    sol::state_view lua(state);
    sol::object ignored = nilObject(lua);
    if (nativeClassProperty(lua, nativeType, key, ignored)) {
        nativeType.raw_set(key, value);
        invalidateClassLookup(lua, nativeType);
        return;
    }
    const sol::table metatable =
        class_native::getObjectMetatable(lua, sol::make_object(lua, nativeType));
    const sol::object original =
        metatable.raw_get<sol::object>(NATIVE_CLASS_NEW_INDEX_FIELD);
    if (original.is<sol::protected_function>()) {
        sol::protected_function_result result =
            original.as<sol::protected_function>()(nativeType, key, value);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return;
    }
    if (original.is<sol::table>()) {
        original.as<sol::table>().raw_set(key, value);
        return;
    }
    nativeType.raw_set(key, value);
}

}  // namespace

bool nativeFallbackMemberEligible(const sol::object& key) {
    if (!key.is<std::string>()) {
        return true;
    }
    const std::string name = key.as<std::string>();
    return !name.starts_with("__") && name != "class_check" &&
           name != "class_cast";
}

std::vector<sol::table> nativeRoots(sol::state_view lua,
                                    const sol::table& classTable) {
    std::vector<sol::table> result;
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        bool covered = false;
        for (const sol::table& root : result) {
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

sol::object nativeObjectForType(sol::state_view lua, const sol::table& fields,
                                const sol::table& nativeType) {
    const sol::object rawObjects =
        fields.raw_get<sol::object>("__nativeObjects");
    if (!rawObjects.is<sol::table>()) {
        return nilObject(lua);
    }
    return rawObjects.as<sol::table>().raw_get<sol::object>(
        nativeTypeName(lua, nativeType));
}

sol::object cachedNativeMethod(sol::state_view lua, sol::table fields,
                               const sol::object& key,
                               const sol::object& method,
                               const sol::object& nativeObject,
                               const sol::table& nativeType,
                               bool objectMember) {
    const sol::object rawCache =
        fields.raw_get<sol::object>(NATIVE_METHOD_CACHE_FIELD);
    sol::table cache = rawCache.is<sol::table>() ? rawCache.as<sol::table>()
                                                 : lua.create_table();
    if (!rawCache.is<sol::table>()) {
        fields.raw_set(NATIVE_METHOD_CACHE_FIELD, cache);
    }
    const sol::object rawEntry = cache.raw_get<sol::object>(key);
    if (rawEntry.is<sol::table>()) {
        const sol::table entry = rawEntry.as<sol::table>();
        const sol::object cachedMethod = entry.raw_get<sol::object>(1);
        const sol::object cachedObject = entry.raw_get<sol::object>(2);
        const sol::object cachedWrapper = entry.raw_get<sol::object>(3);
        if (cachedWrapper.is<sol::function>() &&
            objectsRawEqual(cachedMethod, method) &&
            objectsRawEqual(cachedObject, nativeObject)) {
            return cachedWrapper;
        }
    }
    sol::table entry = lua.create_table();
    const sol::object wrapper = wrapNativeMethod(lua, method, nativeObject);
    entry.raw_set(1, method);
    entry.raw_set(2, nativeObject);
    entry.raw_set(3, wrapper);
    entry.raw_set(4, nativeType);
    entry.raw_set(5, objectMember);
    cache.raw_set(key, entry);
    return wrapper;
}

sol::object findCachedNativeMethod(sol::state_view lua, sol::table fields,
                                   const sol::object& key) {
    const sol::object rawCache =
        fields.raw_get<sol::object>(NATIVE_METHOD_CACHE_FIELD);
    if (!rawCache.is<sol::table>()) {
        return nilObject(lua);
    }
    sol::table cache = rawCache.as<sol::table>();
    const sol::object rawEntry = cache.raw_get<sol::object>(key);
    if (!rawEntry.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::table entry = rawEntry.as<sol::table>();
    const sol::object method = entry.raw_get<sol::object>(1);
    const sol::object nativeObject = entry.raw_get<sol::object>(2);
    const sol::object wrapper = entry.raw_get<sol::object>(3);
    const sol::object rawNativeType = entry.raw_get<sol::object>(4);
    const sol::object rawObjectMember = entry.raw_get<sol::object>(5);
    if (!method.is<sol::function>() || !nativeObject.is<sol::userdata>() ||
        !wrapper.is<sol::function>() || !rawNativeType.is<sol::table>() ||
        !rawObjectMember.is<bool>()) {
        cache.raw_set(key, sol::lua_nil);
        return nilObject(lua);
    }
    sol::object current = nilObject(lua);
    if (rawObjectMember.as<bool>()) {
        current = protectedIndex(lua, nativeObject, key);
    } else {
        current = rawMember(lua, rawNativeType.as<sol::table>(), key);
    }
    if (current.is<sol::function>() && objectsRawEqual(current, method)) {
        return wrapper;
    }
    cache.raw_set(key, sol::lua_nil);
    return nilObject(lua);
}

lua_Integer classLookupVersion(const sol::table& classTable) {
    const sol::object rawVersion =
        classTable.raw_get<sol::object>("__lookupVersion");
    return rawVersion.is<lua_Integer>() ? rawVersion.as<lua_Integer>() : 0;
}

sol::table fastIndexCache(sol::state_view lua, sol::table fields) {
    const sol::object rawCache =
        fields.raw_get<sol::object>(FAST_INDEX_CACHE_FIELD);
    if (rawCache.is<sol::table>()) {
        return rawCache.as<sol::table>();
    }
    sol::table cache = lua.create_table();
    fields.raw_set(FAST_INDEX_CACHE_FIELD, cache);
    return cache;
}

void cacheFastIndex(sol::state_view lua, sol::table fields,
                    const sol::table& classTable, const sol::object& key,
                    FastIndexKind kind, const sol::object& route) {
    sol::table entry = lua.create_table(3, 0);
    entry.raw_set(1, static_cast<lua_Integer>(kind));
    entry.raw_set(2, route);
    entry.raw_set(3, classLookupVersion(classTable));
    fastIndexCache(lua, fields).raw_set(key, entry);
}

void cacheFastClassOwner(sol::state_view lua, sol::table fields,
                         const sol::table& classTable, const sol::object& key,
                         const char* category, FastIndexKind kind) {
    const sol::object rawOwner =
        classLookupOwners(lua, classTable, category).raw_get<sol::object>(key);
    if (rawOwner.is<sol::table>()) {
        cacheFastIndex(lua, fields, classTable, key, kind, rawOwner);
    }
}

}  // namespace ludork::standard::class_runtime::detail

namespace ludork::standard::class_runtime {

void registerNativeClass(sol::table nativeType, const sol::table& metadata) {
    using namespace ludork::standard::class_runtime::detail;
    sol::state_view lua(nativeType.lua_state());
    sol::table defaults = lua.create_table();
    const sol::object rawAttrs = metadata.raw_get<sol::object>("attrs");
    if (rawAttrs.is<sol::table>()) {
        const sol::table attrs = rawAttrs.as<sol::table>();
        for (std::size_t index = 1; index <= attrs.size(); ++index) {
            const sol::object rawName = attrs.raw_get<sol::object>(index);
            if (!rawName.is<std::string>()) {
                continue;
            }
            const sol::object rawField =
                metadata.raw_get<sol::object>(rawName.as<std::string>());
            if (!rawField.is<sol::table>()) {
                continue;
            }
            const sol::object rawDefault =
                rawField.as<sol::table>().raw_get<sol::object>("default");
            if (!rawDefault.valid() ||
                rawDefault.get_type() == sol::type::lua_nil) {
                continue;
            }
            const sol::object value = class_runtime::clonePlainData(lua, rawDefault);
            defaults.raw_set(rawName, value);
        }
    }
    nativeType.raw_set("__classDefaults", defaults);
    nativeType.raw_set(NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD, lua.create_table());

    sol::table metatable =
        class_native::getObjectMetatable(lua, sol::make_object(lua, nativeType));
    const sol::object rawGuard =
        metatable.raw_get<sol::object>(NATIVE_CLASS_GUARD_FIELD);
    if (rawGuard.is<bool>() && rawGuard.as<bool>()) {
        return;
    }
    metatable.raw_set(NATIVE_CLASS_INDEX_FIELD,
                      metatable.raw_get<sol::object>("__index"));
    metatable.raw_set(NATIVE_CLASS_NEW_INDEX_FIELD,
                      metatable.raw_get<sol::object>("__newindex"));
    metatable.set_function("__index", &nativeClassIndex);
    metatable.set_function("__newindex", &nativeClassNewIndex);
    metatable.raw_set(NATIVE_CLASS_GUARD_FIELD, true);
}

void registerNativeClassDefaultResolver(
    sol::state_view lua, const sol::protected_function& callback) {
    lua.registry().raw_set(detail::nativeClassDefaultResolverKey(lua), callback);
}

void unregisterNativeClassDefaultResolver(sol::state_view lua) {
    lua.registry().raw_set(detail::nativeClassDefaultResolverKey(lua), sol::lua_nil);
}

}  // namespace ludork::standard::class_runtime
