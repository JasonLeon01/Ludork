#include "Internal.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Read fast path ────────────────────────────────────────────────────────────

sol::object compositeIndexSlow(sol::object target, sol::object key,
                               sol::this_state state) {
    sol::state_view lua(state);
    const sol::table fields = class_native::getUserFields(lua, target, false);
    if (rawBool(fields, NATIVE_CONSTRUCTION_FAILED_FIELD)) {
        throw std::runtime_error("Class instance construction failed");
    }
    const sol::object field = fields.raw_get<sol::object>(key);
    if (field.valid() && field.get_type() != sol::type::lua_nil) {
        return field;
    }
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    if (!rawClass.is<sol::table>()) {
        return nilObject(lua);
    }
    const sol::table classTable = rawClass.as<sol::table>();
    const sol::object getter = findAccessor(lua, classTable, "__getters", key);
    if (getter.is<sol::function>()) {
        cacheFastClassOwner(lua, fields, classTable, key, "__getters",
                            FastIndexKind::Getter);
        return getter.as<sol::function>()(target);
    }
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
        const bool declaredProperty =
            nativeTypeDeclaresProperty(nativeType, key);
        if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
            continue;
        }
        const sol::object nativeDefinition =
            nativeTypeDefinition(lua, nativeType, key);
        if (!nativeDefinition.valid() ||
            nativeDefinition.get_type() == sol::type::lua_nil) {
            continue;
        }
        if (!declaredProperty && !nativeDefinition.is<sol::function>()) {
            continue;
        }
        sol::object nativeObject = nativeObjectForType(lua, fields, nativeType);
        if (!nativeObject.is<sol::userdata>() && declaredProperty) {
            nativeObject = ensureDefaultNativeObject(lua, target, nativeType);
        }
        if (!nativeObject.is<sol::userdata>()) {
            continue;
        }
        const sol::object nativeValue = protectedIndex(lua, nativeObject, key);
        if (declaredProperty || !nativeValue.is<sol::function>()) {
            cacheFastIndex(
                lua, fields, classTable, key, FastIndexKind::NativeMember,
                sol::make_object(lua, nativeTypeName(lua, nativeType)));
            return nativeValue;
        }
    }
    const sol::object scriptMember = findScriptMember(lua, classTable, key);
    if (scriptMember.valid() && scriptMember.get_type() != sol::type::lua_nil) {
        cacheFastClassOwner(lua, fields, classTable, key, "scriptMembers",
                            FastIndexKind::ScriptMember);
        return scriptMember;
    }
    const sol::object cachedNative = findCachedNativeMethod(lua, fields, key);
    if (cachedNative.is<sol::function>()) {
        cacheFastIndex(lua, fields, classTable, key, FastIndexKind::Value,
                       cachedNative);
        return cachedNative;
    }
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const bool declaredProperty =
            nativeTypeDeclaresProperty(nativeType, key);
        if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
            continue;
        }
        sol::object nativeObject = nativeObjectForType(lua, fields, nativeType);
        sol::object nativeValue = nilObject(lua);
        if (nativeObject.is<sol::userdata>()) {
            const sol::object nativeDefinition =
                nativeTypeDefinition(lua, nativeType, key);
            if (nativeDefinition.valid() &&
                nativeDefinition.get_type() != sol::type::lua_nil &&
                (declaredProperty || nativeDefinition.is<sol::function>())) {
                nativeValue = protectedIndex(lua, nativeObject, key);
                if (!nativeValue.is<sol::function>()) {
                    return nativeValue;
                }
            }
        }
        const sol::object member = rawMember(lua, nativeType, key);
        if (member.valid() && member.get_type() != sol::type::lua_nil) {
            if (isNativeInitializer(nativeType, member)) {
                const sol::object wrapper = cachedNativeMethod(
                    lua, fields, key, member, target, nativeType, false);
                cacheFastIndex(lua, fields, classTable, key,
                               FastIndexKind::Value, wrapper);
                return wrapper;
            }
            if (member.is<sol::function>()) {
                if (!nativeObject.is<sol::userdata>()) {
                    nativeObject =
                        ensureDefaultNativeObject(lua, target, nativeType);
                }
                const sol::object wrapper = cachedNativeMethod(
                    lua, fields, key, member, nativeObject, nativeType, false);
                cacheFastIndex(lua, fields, classTable, key,
                               FastIndexKind::Value, wrapper);
                return wrapper;
            }
            return member;
        }
        if (nativeValue.is<sol::function>()) {
            const sol::object wrapper = cachedNativeMethod(
                lua, fields, key, nativeValue, nativeObject, nativeType, true);
            cacheFastIndex(lua, fields, classTable, key, FastIndexKind::Value,
                           wrapper);
            return wrapper;
        }
    }
    return nilObject(lua);
}

namespace {

int returnTopValue(lua_State* state) {
    lua_replace(state, 1);
    lua_settop(state, 1);
    return 1;
}

}  // namespace

void invalidateFastIndexEntry(lua_State* state, int cacheIndex) {
    lua_pushvalue(state, 2);
    lua_pushnil(state);
    lua_rawset(state, cacheIndex);
}

int compositeIndex(lua_State* state) {
    try {
        if (lua_type(state, 1) == LUA_TUSERDATA &&
            lua_getiuservalue(state, 1, 1) == LUA_TTABLE) {
            const int fieldsIndex = lua_absindex(state, -1);
            lua_getfield(state, fieldsIndex, NATIVE_CONSTRUCTION_FAILED_FIELD);
            const bool constructionFailed = lua_toboolean(state, -1) != 0;
            lua_pop(state, 1);
            if (constructionFailed) {
                return luaL_error(state, "Class instance construction failed");
            }

            lua_pushvalue(state, 2);
            lua_rawget(state, fieldsIndex);
            if (!lua_isnil(state, -1)) {
                return returnTopValue(state);
            }
            lua_pop(state, 1);

            lua_getfield(state, fieldsIndex, FAST_INDEX_CACHE_FIELD);
            if (lua_istable(state, -1)) {
                const int cacheIndex = lua_absindex(state, -1);
                lua_pushvalue(state, 2);
                lua_rawget(state, cacheIndex);
                if (lua_istable(state, -1)) {
                    const int entryIndex = lua_absindex(state, -1);
                    lua_getfield(state, fieldsIndex, "__class");
                    if (lua_istable(state, -1)) {
                        lua_getfield(state, -1, "__lookupVersion");
                        const lua_Integer currentVersion =
                            lua_isinteger(state, -1)
                                ? lua_tointeger(state, -1)
                                : 0;
                        lua_pop(state, 2);
                        lua_rawgeti(state, entryIndex, 3);
                        const lua_Integer cachedVersion =
                            lua_isinteger(state, -1)
                                ? lua_tointeger(state, -1)
                                : -1;
                        lua_pop(state, 1);
                        if (currentVersion == cachedVersion) {
                            lua_rawgeti(state, entryIndex, 1);
                            const FastIndexKind kind =
                                static_cast<FastIndexKind>(
                                    lua_isinteger(state, -1)
                                        ? lua_tointeger(state, -1)
                                        : 0);
                            lua_pop(state, 1);
                            if (kind == FastIndexKind::Value) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (!lua_isnil(state, -1)) {
                                    return returnTopValue(state);
                                }
                                lua_pop(state, 1);
                            } else if (kind == FastIndexKind::ScriptMember) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (lua_istable(state, -1)) {
                                    lua_pushvalue(state, 2);
                                    lua_rawget(state, -2);
                                    if (!lua_isnil(state, -1)) {
                                        return returnTopValue(state);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            } else if (kind == FastIndexKind::Getter) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (lua_istable(state, -1)) {
                                    lua_getfield(state, -1, "__getters");
                                    if (lua_istable(state, -1)) {
                                        lua_pushvalue(state, 2);
                                        lua_rawget(state, -2);
                                        if (lua_isfunction(state, -1)) {
                                            lua_pushvalue(state, 1);
                                            lua_call(state, 1, 1);
                                            return returnTopValue(state);
                                        }
                                        lua_pop(state, 1);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            } else if (kind == FastIndexKind::NativeMember) {
                                lua_getfield(state, fieldsIndex,
                                             "__nativeObjects");
                                if (lua_istable(state, -1)) {
                                    lua_rawgeti(state, entryIndex, 2);
                                    lua_rawget(state, -2);
                                    if (lua_isuserdata(state, -1)) {
                                        lua_pushvalue(state, 2);
                                        lua_gettable(state, -2);
                                        if (!lua_isnil(state, -1)) {
                                            return returnTopValue(state);
                                        }
                                        lua_pop(state, 1);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            }
                        }
                    } else {
                        lua_pop(state, 1);
                    }
                    invalidateFastIndexEntry(state, cacheIndex);
                }
            }
        }
        lua_settop(state, 2);
        sol::state_view lua(state);
        const sol::object target = sol::stack::get<sol::object>(state, 1);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object result = compositeIndexSlow(target, key, state);
        result.push();
        return returnTopValue(state);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

// ── Native property write helpers ─────────────────────────────────────────────

namespace {

bool setNativeObjectMember(sol::state_view lua, const sol::object& nativeObject,
                           const sol::table& nativeType, const sol::object& key,
                           const sol::object& value) {
    if (!nativeObject.is<sol::userdata>()) {
        return false;
    }
    const bool declaredProperty = nativeTypeDeclaresProperty(nativeType, key);
    if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
        return false;
    }
    const sol::object nativeDefinition =
        nativeTypeDefinition(lua, nativeType, key);
    if (!nativeDefinition.valid() ||
        nativeDefinition.get_type() == sol::type::lua_nil) {
        return false;
    }
    if (!declaredProperty && !nativeDefinition.is<sol::function>()) {
        return false;
    }
    const sol::object current = protectedIndex(lua, nativeObject, key);
    if (!declaredProperty && current.is<sol::function>()) {
        return false;
    }
    protectedAssign(lua, nativeObject, key, value);
    return true;
}

bool setNativeMember(sol::state_view lua, const sol::table& fields,
                     const sol::table& classTable, const sol::object& key,
                     const sol::object& value,
                     sol::object* assignedObject = nullptr) {
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
        const sol::object nativeObject =
            nativeObjectForType(lua, fields, nativeType);
        if (setNativeObjectMember(lua, nativeObject, nativeType, key, value)) {
            if (assignedObject != nullptr) {
                *assignedObject = nativeObject;
            }
            return true;
        }
    }
    return false;
}

}  // namespace

void markNativePropertyDirty(sol::state_view lua, sol::table fields,
                             const sol::object& nativeObject,
                             const sol::object& key) {
    if (!rawBool(fields, NATIVE_INITIALIZING_FIELD)) {
        return;
    }
    const sol::object rawDirty =
        fields.raw_get<sol::object>(NATIVE_DIRTY_PROPERTIES_FIELD);
    sol::table dirty = rawDirty.is<sol::table>() ? rawDirty.as<sol::table>()
                                                 : lua.create_table();
    if (!rawDirty.is<sol::table>()) {
        fields.raw_set(NATIVE_DIRTY_PROPERTIES_FIELD, dirty);
    }
    const sol::object rawProperties = dirty.raw_get<sol::object>(nativeObject);
    sol::table properties = rawProperties.is<sol::table>()
                                ? rawProperties.as<sol::table>()
                                : lua.create_table();
    if (!rawProperties.is<sol::table>()) {
        dirty.raw_set(nativeObject, properties);
    }
    properties.raw_set(key, true);
}

void restoreNativeShadows(sol::table fields,
                          const NativeShadowSnapshot& snapshot) {
    for (auto iterator = snapshot.rbegin(); iterator != snapshot.rend();
         ++iterator) {
        fields.raw_set(iterator->first, iterator->second);
    }
}

void syncNativeRootDefaults(sol::state_view lua, const sol::table& classTable,
                            const sol::object& instance,
                            const sol::table& root,
                            const sol::object& nativeObject,
                            NativeShadowSnapshot& shadowSnapshot) {
    sol::table fields = class_native::getUserFields(lua, instance, false);
    std::vector<std::string> properties;
    std::unordered_set<std::string> seenProperties;
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::object rawProperties =
            rawType.as<sol::table>().raw_get<sol::object>("__nativeProperties");
        if (!rawProperties.is<sol::table>()) {
            continue;
        }
        const sol::table nativeProperties = rawProperties.as<sol::table>();
        for (std::size_t propertyIndex = 1;
             propertyIndex <= nativeProperties.size(); ++propertyIndex) {
            const sol::object rawName =
                nativeProperties.raw_get<sol::object>(propertyIndex);
            if (rawName.is<std::string>()) {
                const std::string name = rawName.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    const sol::table classMro = getMro(lua, classTable);
    for (std::size_t index = 1; index <= classMro.size(); ++index) {
        const sol::object rawType = classMro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>() || !isClass(rawType.as<sol::table>())) {
            continue;
        }
        for (const auto& entry : rawType.as<sol::table>()) {
            if (entry.first.is<std::string>() &&
                nativeFallbackMemberEligible(entry.first)) {
                const std::string name = entry.first.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    for (const std::string& property : properties) {
        const sol::object key = sol::make_object(lua, property);
        const sol::object shadow = fields.raw_get<sol::object>(key);
        const bool hasShadow =
            shadow.valid() && shadow.get_type() != sol::type::lua_nil;
        const sol::object value =
            hasShadow ? shadow : findClassOverride(lua, classTable, key);
        if (!value.valid() || value.get_type() == sol::type::lua_nil) {
            continue;
        }
        bool assigned = false;
        for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
            const sol::object rawType = nativeMro.raw_get<sol::object>(index);
            if (!rawType.is<sol::table>()) {
                continue;
            }
            if (setNativeObjectMember(lua, nativeObject,
                                      rawType.as<sol::table>(), key, value)) {
                assigned = true;
                break;
            }
        }
        if (assigned && hasShadow) {
            shadowSnapshot.emplace_back(key, shadow);
            fields.raw_set(key, sol::lua_nil);
        }
    }
}

void replayNativeDirtyProperties(sol::state_view lua, const sol::table& fields,
                                 const sol::table& root,
                                 const sol::object& source,
                                 const sol::object& destination) {
    if (!source.is<sol::userdata>() || !destination.is<sol::userdata>()) {
        return;
    }
    const sol::object rawDirty =
        fields.raw_get<sol::object>(NATIVE_DIRTY_PROPERTIES_FIELD);
    if (!rawDirty.is<sol::table>()) {
        return;
    }
    const sol::object rawProperties =
        rawDirty.as<sol::table>().raw_get<sol::object>(source);
    if (!rawProperties.is<sol::table>()) {
        return;
    }
    const sol::table nativeMro = getNativeMro(lua, root);
    for (const auto& entry : rawProperties.as<sol::table>()) {
        if (!entry.second.is<bool>() || !entry.second.as<bool>()) {
            continue;
        }
        const sol::object key = entry.first;
        const sol::object value = protectedIndex(lua, source, key);
        for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
            const sol::object rawType = nativeMro.raw_get<sol::object>(index);
            if (rawType.is<sol::table>() &&
                setNativeObjectMember(lua, destination,
                                      rawType.as<sol::table>(), key, value)) {
                break;
            }
        }
    }
}

void syncNativeClassDefaults(sol::state_view lua, const sol::table& classTable,
                             const sol::object& instance) {
    if (instance.get_type() != sol::type::userdata) {
        return;
    }
    const sol::table fields =
        class_native::getUserFields(lua, instance, false);
    if (!fields.raw_get<sol::object>("__nativeObjects").is<sol::table>()) {
        return;
    }
    std::vector<std::string> properties;
    std::unordered_set<std::string> seenProperties;
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const sol::object rawProperties =
            nativeType.raw_get<sol::object>("__nativeProperties");
        if (!rawProperties.is<sol::table>()) {
            continue;
        }
        const sol::table nativeProperties = rawProperties.as<sol::table>();
        for (std::size_t propertyIndex = 1;
             propertyIndex <= nativeProperties.size(); ++propertyIndex) {
            const sol::object rawName =
                nativeProperties.raw_get<sol::object>(propertyIndex);
            if (rawName.is<std::string>()) {
                const std::string name = rawName.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    for (const std::string& property : properties) {
        const sol::object key = sol::make_object(lua, property);
        const sol::object value = findClassOverride(lua, classTable, key);
        if (value.valid() && value.get_type() != sol::type::lua_nil) {
            setNativeMember(lua, fields, classTable, key, value);
        }
    }
}

// ── Monitor ───────────────────────────────────────────────────────────────────

namespace {

sol::object monitorMissing(sol::state_view lua) {
    const sol::object rawClass = lua.globals().raw_get<sol::object>("Class");
    if (rawClass.is<sol::table>()) {
        sol::table classModule = rawClass.as<sol::table>();
        const sol::object missing = classModule.raw_get<sol::object>("MISSING");
        if (missing.is<sol::table>()) {
            return missing;
        }
        sol::table created = lua.create_table();
        classModule.raw_set("MISSING", created);
        return created;
    }
    return lua.create_table();
}

sol::table monitorState(sol::state_view lua, const sol::object& target) {
    const sol::object rawState = registryTable(lua, MONITOR_STATES_KEY, "k")
                                     .raw_get<sol::object>(target);
    return rawState.is<sol::table>() ? rawState.as<sol::table>()
                                     : lua.create_table();
}

sol::object originalMonitoredIndex(sol::state_view lua,
                                   const sol::table& state,
                                   const sol::object& target,
                                   const sol::object& key) {
    const sol::object originalIndex = state.raw_get<sol::object>("index");
    if (originalIndex.is<sol::protected_function>()) {
        sol::protected_function_result result =
            originalIndex.as<sol::protected_function>()(target, key);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return result.get<sol::object>();
    }
    if (originalIndex.is<sol::table>()) {
        return originalIndex.as<sol::table>().raw_get<sol::object>(key);
    }
    return target.as<sol::table>().raw_get<sol::object>(key);
}

void originalMonitoredNewIndex(sol::state_view lua, const sol::table& state,
                               const sol::object& target,
                               const sol::object& key,
                               const sol::object& value) {
    const sol::object originalNewIndex = state.raw_get<sol::object>("newIndex");
    if (originalNewIndex.is<sol::protected_function>()) {
        sol::protected_function_result result =
            originalNewIndex.as<sol::protected_function>()(target, key, value);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return;
    }
    if (originalNewIndex.is<sol::table>()) {
        originalNewIndex.as<sol::table>().raw_set(key, value);
        return;
    }
    target.as<sol::table>().raw_set(key, value);
}

void invokeMonitorCallback(sol::state_view lua, sol::table entry,
                           const sol::object& oldValue,
                           const sol::object& newValue) {
    if (luaValuesEqual(lua, oldValue, newValue)) {
        return;
    }
    const sol::object rawRunning = entry.raw_get<sol::object>("running");
    if (rawRunning.is<bool>() && rawRunning.as<bool>()) {
        return;
    }
    const sol::object rawCallback = entry.raw_get<sol::object>("callback");
    if (!rawCallback.is<sol::protected_function>()) {
        return;
    }
    std::vector<sol::object> arguments{oldValue, newValue};
    const sol::object rawParams = entry.raw_get<sol::object>("params");
    if (rawParams.is<sol::table>()) {
        const sol::table params = rawParams.as<sol::table>();
        if (params.size() >
            static_cast<std::size_t>(INT_MAX) - arguments.size()) {
            throw std::length_error("Monitor callback argument count overflow");
        }
        arguments.reserve(arguments.size() + params.size());
        for (std::size_t index = 1; index <= params.size(); ++index) {
            arguments.push_back(params.raw_get<sol::object>(index));
        }
    }
    entry.raw_set("running", true);
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        static_cast<void>(invokeRuntimeFunction(
            lua, rawCallback, arguments, "monitor callback arguments"));
        lua_settop(state, stackBase);
        entry.raw_set("running", false);
    } catch (...) {
        lua_settop(state, stackBase);
        entry.raw_set("running", false);
        throw;
    }
}

sol::object monitoredTableIndex(sol::object target, sol::object key,
                                sol::this_state state) {
    sol::state_view lua(state);
    const sol::table monitor = monitorState(lua, target);
    const sol::object rawFields = monitor.raw_get<sol::object>("fields");
    if (rawFields.is<sol::table>()) {
        const sol::object rawEntry =
            rawFields.as<sol::table>().raw_get<sol::object>(key);
        if (rawEntry.is<sol::table>()) {
            const sol::table entry = rawEntry.as<sol::table>();
            const sol::object rawHasValue =
                entry.raw_get<sol::object>("hasValue");
            if (rawHasValue.is<bool>() && rawHasValue.as<bool>()) {
                return entry.raw_get<sol::object>("value");
            }
            return nilObject(lua);
        }
    }
    return originalMonitoredIndex(lua, monitor, target, key);
}

void monitoredTableNewIndex(sol::object target, sol::object key,
                            sol::object value, sol::this_state state) {
    sol::state_view lua(state);
    const sol::table monitor = monitorState(lua, target);
    const sol::object rawFields = monitor.raw_get<sol::object>("fields");
    const sol::object rawEntry =
        rawFields.is<sol::table>()
            ? rawFields.as<sol::table>().raw_get<sol::object>(key)
            : nilObject(lua);
    if (!rawEntry.is<sol::table>()) {
        originalMonitoredNewIndex(lua, monitor, target, key, value);
        return;
    }
    if (!value.valid() || value.get_type() == sol::type::lua_nil) {
        throw std::invalid_argument("Monitored fields cannot be assigned nil");
    }
    sol::table entry = rawEntry.as<sol::table>();
    const sol::object rawHasValue = entry.raw_get<sol::object>("hasValue");
    const sol::object oldValue =
        rawHasValue.is<bool>() && rawHasValue.as<bool>()
            ? entry.raw_get<sol::object>("value")
            : entry.raw_get<sol::object>("missing");
    entry.raw_set("value", value);
    entry.raw_set("hasValue", true);
    entry.raw_set("assigned", true);
    invokeMonitorCallback(lua, entry, oldValue, value);
}

sol::table createTableMonitorState(sol::state_view lua, sol::table target) {
    lua_State* state = lua.lua_state();
    target.push();
    sol::object originalMetatable = nilObject(lua);
    if (lua_getmetatable(state, -1) != 0) {
        originalMetatable = sol::stack::get<sol::object>(state, -1);
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    if (originalMetatable.is<sol::table>()) {
        const sol::object protection =
            originalMetatable.as<sol::table>().raw_get<sol::object>(
                "__metatable");
        if (protection.valid() && protection.get_type() != sol::type::lua_nil) {
            throw std::invalid_argument(
                "Lua monitors cannot replace a protected metatable");
        }
    }
    sol::table monitor = lua.create_table();
    sol::table fields = lua.create_table();
    monitor.raw_set("meta", originalMetatable);
    monitor.raw_set("fields", fields);
    if (originalMetatable.is<sol::table>()) {
        sol::table original = originalMetatable.as<sol::table>();
        monitor.raw_set("index", original.raw_get<sol::object>("__index"));
        monitor.raw_set("newIndex",
                        original.raw_get<sol::object>("__newindex"));
    }
    sol::table proxy = lua.create_table();
    proxy.set_function("__index", &monitoredTableIndex);
    proxy.set_function("__newindex", &monitoredTableNewIndex);
    target.push();
    proxy.push();
    lua_setmetatable(state, -2);
    lua_pop(state, 1);
    registryTable(lua, MONITOR_STATES_KEY, "k").raw_set(target, monitor);
    return monitor;
}

}  // namespace

void registerMonitor(sol::this_state state, const sol::object& target,
                     const std::string& name,
                     const sol::protected_function& callback,
                     sol::optional<sol::table> params) {
    sol::state_view lua(state);
    if (name.empty()) {
        throw std::invalid_argument("Monitor field name must not be empty");
    }
    if (target.get_type() == sol::type::table) {
        sol::table object = target.as<sol::table>();
        sol::table monitor = monitorState(lua, target);
        if (!monitor.raw_get<sol::object>("fields").is<sol::table>()) {
            monitor = createTableMonitorState(lua, object);
        }
        sol::table fields = monitor.raw_get<sol::table>("fields");
        const sol::object rawEntry = fields.raw_get<sol::object>(name);
        if (rawEntry.is<sol::table>()) {
            sol::table entry = rawEntry.as<sol::table>();
            entry.raw_set("callback", callback);
            entry.raw_set("params", params.value_or(lua.create_table()));
            return;
        }
        const sol::object rawValue = object.raw_get<sol::object>(name);
        sol::object value = rawValue;
        if (!value.valid() || value.get_type() == sol::type::lua_nil) {
            value = originalMonitoredIndex(lua, monitor, target,
                                           sol::make_object(lua, name));
        }
        sol::table entry = lua.create_table();
        const bool hasValue =
            value.valid() && value.get_type() != sol::type::lua_nil;
        entry.raw_set("hasValue", hasValue);
        if (hasValue) {
            entry.raw_set("value", value);
        }
        entry.raw_set("callback", callback);
        entry.raw_set("params", params.value_or(lua.create_table()));
        entry.raw_set("running", false);
        entry.raw_set("raw", rawValue.valid() &&
                                 rawValue.get_type() != sol::type::lua_nil);
        entry.raw_set("assigned", false);
        entry.raw_set("missing", monitorMissing(lua));
        fields.raw_set(name, entry);
        object.raw_set(name, sol::lua_nil);
        return;
    }
    if (target.get_type() != sol::type::userdata) {
        throw std::invalid_argument(
            "Monitors require a table or userdata target");
    }
    sol::table fields = class_native::getUserFields(lua, target, true);
    const sol::object rawCallbacks =
        fields.raw_get<sol::object>("__monitorCallbacks");
    sol::table callbacks = rawCallbacks.is<sol::table>()
                               ? rawCallbacks.as<sol::table>()
                               : lua.create_table();
    if (!rawCallbacks.is<sol::table>()) {
        fields.raw_set("__monitorCallbacks", callbacks);
    }
    sol::table entry = lua.create_table();
    entry.raw_set("callback", callback);
    entry.raw_set("params", params.value_or(lua.create_table()));
    entry.raw_set("running", false);
    entry.raw_set("missing", monitorMissing(lua));
    callbacks.raw_set(name, entry);
}

void unregisterMonitor(sol::this_state state, const sol::object& target,
                       const std::string& name) {
    sol::state_view lua(state);
    if (target.get_type() == sol::type::table) {
        sol::table object = target.as<sol::table>();
        sol::table monitor = monitorState(lua, target);
        const sol::object rawFields = monitor.raw_get<sol::object>("fields");
        if (!rawFields.is<sol::table>()) {
            return;
        }
        sol::table fields = rawFields.as<sol::table>();
        const sol::object rawEntry = fields.raw_get<sol::object>(name);
        if (!rawEntry.is<sol::table>()) {
            return;
        }
        sol::table entry = rawEntry.as<sol::table>();
        fields.raw_set(name, sol::lua_nil);
        const bool restore =
            rawBool(entry, "raw") || rawBool(entry, "assigned");
        if (restore) {
            const bool hasValue = rawBool(entry, "hasValue");
            object.raw_set(name, hasValue ? entry.raw_get<sol::object>("value")
                                          : nilObject(lua));
        }
        if (!tableIsEmpty(fields)) {
            return;
        }
        const sol::object originalMetatable =
            monitor.raw_get<sol::object>("meta");
        object.push();
        if (originalMetatable.valid() &&
            originalMetatable.get_type() != sol::type::lua_nil) {
            originalMetatable.push();
        } else {
            lua_pushnil(lua.lua_state());
        }
        lua_setmetatable(lua.lua_state(), -2);
        lua_pop(lua.lua_state(), 1);
        registryTable(lua, MONITOR_STATES_KEY, "k")
            .raw_set(object, sol::lua_nil);
        return;
    }
    if (target.get_type() != sol::type::userdata) {
        return;
    }
    sol::table fields = class_native::getUserFields(lua, target, true);
    const sol::object rawCallbacks =
        fields.raw_get<sol::object>("__monitorCallbacks");
    if (!rawCallbacks.is<sol::table>()) {
        return;
    }
    sol::table callbacks = rawCallbacks.as<sol::table>();
    callbacks.raw_set(name, sol::lua_nil);
    if (tableIsEmpty(callbacks)) {
        fields.raw_set("__monitorCallbacks", sol::lua_nil);
    }
}

// ── Write fast path (task 1) ──────────────────────────────────────────────────

// Slow write path: handles setter, native member, plain field, and monitor
// callbacks. Does NOT clear FAST_INDEX_CACHE; instead populates it on success.
namespace {

void compositeNewIndexSlow(lua_State* state, const sol::object& target,
                           const sol::object& key, const sol::object& value) {
    sol::state_view lua(state);
    sol::table fields = class_native::getUserFields(lua, target, true);
    if (rawBool(fields, NATIVE_CONSTRUCTION_FAILED_FIELD)) {
        throw std::runtime_error("Class instance construction failed");
    }
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    auto assignValue = [&]() {
        if (!rawClass.is<sol::table>()) {
            fields.raw_set(key, value);
            return;
        }
        const sol::table classTable = rawClass.as<sol::table>();
        const sol::object setter =
            findAccessor(lua, classTable, "__setters", key);
        if (setter.is<sol::function>()) {
            setter.as<sol::function>()(target, value);
            cacheFastClassOwner(lua, fields, classTable, key, "__getters",
                                FastIndexKind::Getter);
            return;
        }
        sol::object assignedObject = nilObject(lua);
        if (setNativeMember(lua, fields, classTable, key, value,
                            &assignedObject)) {
            markNativePropertyDirty(lua, fields, assignedObject, key);
            const sol::object rawType = nativeTypeOf(lua, assignedObject);
            if (rawType.is<sol::table>()) {
                cacheFastIndex(
                    lua, fields, classTable, key, FastIndexKind::NativeMember,
                    sol::make_object(
                        lua, nativeTypeName(lua, rawType.as<sol::table>())));
            }
            return;
        }
        fields.raw_set(key, value);
    };
    const sol::object rawCallbacks =
        fields.raw_get<sol::object>("__monitorCallbacks");
    if (!rawCallbacks.is<sol::table>()) {
        assignValue();
        return;
    }
    const sol::object rawEntry =
        rawCallbacks.as<sol::table>().raw_get<sol::object>(key);
    if (!rawEntry.is<sol::table>()) {
        assignValue();
        return;
    }
    sol::table entry = rawEntry.as<sol::table>();
    const sol::object rawRunning = entry.raw_get<sol::object>("running");
    if (rawRunning.is<bool>() && rawRunning.as<bool>()) {
        assignValue();
        return;
    }
    if (!value.valid() || value.get_type() == sol::type::lua_nil) {
        throw std::invalid_argument("Monitored fields cannot be assigned nil");
    }
    sol::object oldValue = compositeIndexSlow(target, key, state);
    if (!oldValue.valid() || oldValue.get_type() == sol::type::lua_nil) {
        oldValue = entry.raw_get<sol::object>("missing");
    }
    assignValue();
    invokeMonitorCallback(lua, entry, oldValue, value);
}

}  // namespace

// C fast path: NativeMember and Getter cache hits bypass sol entirely.
// Degrades to compositeNewIndexSlow for monitors, cache misses, or
// initializing state (where dirty tracking is needed).
int compositeNewIndex(lua_State* state) {
    try {
        if (lua_type(state, 1) == LUA_TUSERDATA &&
            lua_getiuservalue(state, 1, 1) == LUA_TTABLE) {
            const int fieldsIndex = lua_absindex(state, -1);

            lua_getfield(state, fieldsIndex, NATIVE_CONSTRUCTION_FAILED_FIELD);
            const bool failed = lua_toboolean(state, -1) != 0;
            lua_pop(state, 1);
            if (failed) {
                return luaL_error(state, "Class instance construction failed");
            }

            // Skip fast path when there is an active (non-running) monitor
            bool activeMonitor = false;
            lua_getfield(state, fieldsIndex, "__monitorCallbacks");
            if (lua_istable(state, -1)) {
                lua_pushvalue(state, 2);
                lua_rawget(state, -2);
                if (lua_istable(state, -1)) {
                    lua_getfield(state, -1, "running");
                    activeMonitor = !lua_toboolean(state, -1);
                    lua_pop(state, 1);
                }
                lua_pop(state, 1);
            }
            lua_pop(state, 1);

            if (!activeMonitor) {
                lua_getfield(state, fieldsIndex, FAST_INDEX_CACHE_FIELD);
                if (lua_istable(state, -1)) {
                    const int cacheIndex = lua_absindex(state, -1);
                    lua_pushvalue(state, 2);
                    lua_rawget(state, cacheIndex);
                    if (lua_istable(state, -1)) {
                        const int entryIndex = lua_absindex(state, -1);
                        lua_getfield(state, fieldsIndex, "__class");
                        lua_Integer currentVersion = 0;
                        if (lua_istable(state, -1)) {
                            lua_getfield(state, -1, "__lookupVersion");
                            currentVersion =
                                lua_isinteger(state, -1)
                                    ? lua_tointeger(state, -1)
                                    : 0;
                            lua_pop(state, 1);
                        }
                        lua_pop(state, 1);
                        lua_rawgeti(state, entryIndex, 3);
                        const lua_Integer cachedVersion =
                            lua_isinteger(state, -1)
                                ? lua_tointeger(state, -1)
                                : -1;
                        lua_pop(state, 1);
                        if (currentVersion == cachedVersion) {
                            lua_rawgeti(state, entryIndex, 1);
                            const FastIndexKind kind =
                                static_cast<FastIndexKind>(
                                    lua_isinteger(state, -1)
                                        ? lua_tointeger(state, -1)
                                        : 0);
                            lua_pop(state, 1);
                            if (kind == FastIndexKind::NativeMember) {
                                // Skip during initialisation (needs dirty tracking)
                                lua_getfield(state, fieldsIndex,
                                             NATIVE_INITIALIZING_FIELD);
                                const bool initializing =
                                    lua_toboolean(state, -1) != 0;
                                lua_pop(state, 1);
                                if (!initializing) {
                                    lua_getfield(state, fieldsIndex,
                                                 "__nativeObjects");
                                    if (lua_istable(state, -1)) {
                                        const int objectsIdx =
                                            lua_absindex(state, -1);
                                        lua_rawgeti(state, entryIndex, 2);
                                        lua_rawget(state, objectsIdx);
                                        if (lua_isuserdata(state, -1)) {
                                            lua_pushvalue(state, 2);
                                            lua_pushvalue(state, 3);
                                            lua_settable(state, -3);
                                            lua_settop(state, 0);
                                            return 0;
                                        }
                                        lua_pop(state, 1);
                                    }
                                    lua_pop(state, 1);
                                }
                                invalidateFastIndexEntry(state, cacheIndex);
                            } else if (kind == FastIndexKind::Getter) {
                                lua_rawgeti(state, entryIndex, 2);
                                if (lua_istable(state, -1)) {
                                    lua_getfield(state, -1, "__setters");
                                    if (lua_istable(state, -1)) {
                                        lua_pushvalue(state, 2);
                                        lua_rawget(state, -2);
                                        if (lua_isfunction(state, -1)) {
                                            lua_pushvalue(state, 1);
                                            lua_pushvalue(state, 3);
                                            lua_call(state, 2, 0);
                                            lua_settop(state, 0);
                                            return 0;
                                        }
                                        lua_pop(state, 1);
                                    }
                                    lua_pop(state, 1);
                                }
                                lua_pop(state, 1);
                            }
                        } else {
                            invalidateFastIndexEntry(state, cacheIndex);
                        }
                    }
                    lua_pop(state, 1);
                }
                lua_pop(state, 1);
            }
        }
        lua_settop(state, 3);
        sol::state_view lua(state);
        compositeNewIndexSlow(state,
                              sol::stack::get<sol::object>(state, 1),
                              sol::stack::get<sol::object>(state, 2),
                              sol::stack::get<sol::object>(state, 3));
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

// ── Composite metatable creation ──────────────────────────────────────────────

namespace {

sol::table createCompositeMetatable(sol::state_view lua, const char* key,
                                    bool finalized) {
    sol::table registry = lua.registry();
    const sol::object rawMetatable = registry.raw_get<sol::object>(key);
    if (rawMetatable.is<sol::table>()) {
        return rawMetatable.as<sol::table>();
    }
    sol::table metatable = lua.create_table();
    metatable.raw_set("__LuaSFNativeComposite", true);
    metatable.push();
    lua_pushcfunction(lua.lua_state(), compositeIndex);
    lua_setfield(lua.lua_state(), -2, "__index");
    lua_pushcfunction(lua.lua_state(), compositeNewIndex);
    lua_setfield(lua.lua_state(), -2, "__newindex");
    lua_pop(lua.lua_state(), 1);
    if (finalized) {
        metatable.push();
        lua_pushcfunction(lua.lua_state(), classInstanceGc);
        lua_setfield(lua.lua_state(), -2, "__gc");
        lua_pop(lua.lua_state(), 1);
    }
    registry.raw_set(key, metatable);
    return metatable;
}

}  // namespace

sol::table compositeMetatable(sol::state_view lua) {
    return createCompositeMetatable(lua, COMPOSITE_METATABLE_KEY, true);
}

sol::table constructingCompositeMetatable(sol::state_view lua) {
    return createCompositeMetatable(lua,
                                    CONSTRUCTING_COMPOSITE_METATABLE_KEY,
                                    false);
}

bool isCompositeInstance(sol::state_view lua, const sol::object& instance) {
    if (!instance.is<sol::userdata>()) {
        return false;
    }
    const sol::object marker =
        class_native::getObjectMetatable(lua, instance)
            .raw_get<sol::object>("__LuaSFNativeComposite");
    return marker.is<bool>() && marker.as<bool>();
}

}  // namespace ludork::standard::class_runtime::detail
