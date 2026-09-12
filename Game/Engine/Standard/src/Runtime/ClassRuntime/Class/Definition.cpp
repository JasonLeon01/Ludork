#include "Class/ClassRuntimeInternals.hpp"
#include "Detail/RuntimeState.hpp"

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/TypeQueries.hpp"
#include "Detail/TypedFields.hpp"
#include "Instance/InstanceRuntime.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
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

// ── Class instance metamethods
// ────────────────────────────────────────────────

namespace {

int classInstanceIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        const sol::table classTable = constructorClass(state);
        const sol::object target = sol::stack::get<sol::object>(state, 1);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object disposeMethod =
            instanceDisposeMethod(lua, classTable, key);
        if (disposeMethod.is<sol::function>()) {
            disposeMethod.push();
            return 1;
        }
        const sol::object getter =
            findAccessor(lua, classTable, protocol::CLASS_GETTERS_FIELD, key);
        if (getter.is<sol::function>()) {
            getter.push();
            target.push();
            lua_call(state, 1, 1);
            return 1;
        }
        if (hasExplicitNilField(lua, target, key)) {
            lua_pushnil(state);
        } else {
            findInClass(lua, classTable, key).push();
        }
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classInstanceNewIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        sol::table classTable = constructorClass(state);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object setter =
            findAccessor(lua, classTable, protocol::CLASS_SETTERS_FIELD, key);
        if (setter.is<sol::function>()) {
            setter.push();
            lua_pushvalue(state, 1);
            lua_pushvalue(state, 3);
            lua_call(state, 2, 0);
            clearExplicitNilField(state, 1, 2);
            return 0;
        }
        lua_pushvalue(state, 2);
        lua_pushvalue(state, 3);
        lua_rawset(state, 1);
        clearExplicitNilField(state, 1, 2);
        invalidateClassLookup(lua, classTable);
        const sol::object value = sol::stack::get<sol::object>(state, 3);
        if (value.is<sol::function>()) {
            const sol::object implementationOwner =
                classTable.raw_get<sol::object>("_hasImplementationOwner");
            if (implementationOwner.valid() &&
                implementationOwner.get_type() != sol::type::lua_nil) {
                classTable.raw_set("_hasImplementationOwner", sol::lua_nil);
            }
        }
        registerMethodOwner(lua, classTable, value);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classMetatableIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        if (hasExplicitNilField(state, 1, 2)) {
            lua_pushnil(state);
        } else {
            findInClass(lua, constructorClass(state), key, false).push();
        }
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classMetatableNewIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        sol::table classTable = constructorClass(state);
        const sol::object value = sol::stack::get<sol::object>(state, 3);
        lua_pushvalue(state, 2);
        lua_pushvalue(state, 3);
        lua_rawset(state, 1);
        clearExplicitNilField(state, 1, 2);
        invalidateClassLookup(lua, classTable);
        if (value.is<sol::function>()) {
            const sol::object implementationOwner =
                classTable.raw_get<sol::object>("_hasImplementationOwner");
            if (implementationOwner.valid() &&
                implementationOwner.get_type() != sol::type::lua_nil) {
                classTable.raw_set("_hasImplementationOwner", sol::lua_nil);
            }
        }
        registerMethodOwner(lua, classTable, value);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

bool isFinalizedClass(const sol::table& value) {
    return isClass(value) && tableHasMetatable(value) &&
           value.raw_get<sol::object>(BASES_FIELD).is<sol::table>() &&
           value.raw_get<sol::object>(MRO_FIELD).is<sol::table>() &&
           value.raw_get<sol::object>("__index").is<sol::function>() &&
           value.raw_get<sol::object>("__newindex").is<sol::function>() &&
           value.raw_get<sol::object>("new").is<sol::function>();
}

constexpr const char* CLASS_RESERVED_FIELDS[] = {
    protocol::CLASS_MARKER_FIELD,
    protocol::CLASS_NAME_FIELD,
    BASES_FIELD,
    protocol::CLASS_BASE_FIELD,
    MRO_FIELD,
    MRO_SET_FIELD,
    RUNTIME_BASES_FIELD,
    RUNTIME_MRO_FIELD,
    RUNTIME_MRO_SET_FIELD,
    NATIVE_BASES_FIELD,
    NATIVE_MRO_FIELD,
    NATIVE_MRO_SET_FIELD,
    SUBCLASSES_FIELD,
    LOOKUP_CACHE_FIELD,
    LOOKUP_VERSION_FIELD,
    "__index",
    "__newindex",
    "__gc",
    "__call",
    "new",
    "_hasImplementationOwner",
    CLASS_BASE_METHODS_FIELD,
    CLASS_CALLBACKS_FIELD,
    CLASS_DEFAULTS_FIELD,
    NATIVE_CLASS_RESOLVED_DEFAULTS_FIELD,
    CLASS_FACTORY_FIELD,
    CLASS_FACTORY_MIN_ARGUMENTS_FIELD,
    NATIVE_INITIALIZER_FIELD,
    NATIVE_METHOD_CACHE_FIELD,
    protocol::NATIVE_OBJECTS_FIELD,
    NATIVE_PROPERTIES_FIELD,
};

void validateClassDefinition(const sol::table& definition) {
    if (rawBool(definition, protocol::CLASS_MARKER_FIELD)) {
        throw std::invalid_argument("Class definition is already finalized");
    }
    if (tableHasMetatable(definition)) {
        throw std::invalid_argument(
            "Class definition must be a plain table without a metatable");
    }
    for (const char* name : CLASS_RESERVED_FIELDS) {
        const sol::object value = definition.raw_get<sol::object>(name);
        if (value.valid() && value.get_type() != sol::type::lua_nil) {
            throw std::invalid_argument(
                "Class definition contains reserved field '" +
                std::string(name) + "'");
        }
    }
}

sol::table normalizeClassBases(sol::state_view lua, const sol::table& bases) {
    sol::table result = lua.create_table();
    std::vector<sol::table> accepted;
    accepted.reserve(bases.size());
    for (std::size_t index = 1; index <= bases.size(); ++index) {
        const sol::object rawBase = bases.raw_get<sol::object>(index);
        if (!rawBase.is<sol::table>()) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        const sol::table base = rawBase.as<sol::table>();
        if (!isFinalizedClass(base) && !isNativeType(lua, base)) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        bool duplicate = false;
        for (const sol::table& existing : accepted) {
            if (objectsRawEqual(existing, base)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            accepted.push_back(base);
            result.add(base);
        }
    }
    return result;
}

}  // namespace

void setClassClosure(lua_State* state, const sol::table& target,
                     const char* name, const sol::table& classTable,
                     lua_CFunction function) {
    target.push();
    lua_pushstring(state, name);
    classTable.push();
    lua_pushcclosure(state, function, 1);
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

// ── Class finalization
// ────────────────────────────────────────────────────────

sol::table finalizeClassImpl(sol::table definition, const sol::table& bases) {
    sol::state_view lua(definition.lua_state());
    validateClassDefinition(definition);
    const sol::table baseList = normalizeClassBases(lua, bases);
    const std::vector<sol::table> linearization =
        createMro(definition, baseList, MroKind::Runtime);
    for (std::size_t index = 1; index < linearization.size(); ++index) {
        if (isNativeType(lua, linearization[index])) {
            ensureNativeInitializer(lua, linearization[index]);
        }
    }
    std::vector<sol::object> ownMethods;
    for (const auto& entry : definition) {
        if (entry.second.is<sol::function>()) {
            ownMethods.push_back(entry.second);
        }
    }
    sol::table mro = lua.create_table();
    for (const sol::table& type : linearization) {
        mro.add(type);
    }
    sol::table classTable = definition;
    classTable.raw_set(protocol::CLASS_MARKER_FIELD, true);
    classTable.raw_set(LOOKUP_VERSION_FIELD, 1);
    classTable.raw_set(BASES_FIELD, baseList);
    if (baseList.size() > 0) {
        classTable.raw_set(protocol::CLASS_BASE_FIELD, baseList[1]);
    }
    classTable.raw_set(MRO_FIELD, mro);
    ensureMroSet(lua, classTable, mro, MRO_SET_FIELD);
    for (const sol::table& base : tableList(baseList)) {
        registerSubclass(lua, base, classTable);
    }
    setClassClosure(lua.lua_state(), classTable, "__index", classTable,
                    classInstanceIndex);
    setClassClosure(lua.lua_state(), classTable, "__newindex", classTable,
                    classInstanceNewIndex);
    setClassClosure(lua.lua_state(), classTable, "__gc", classTable,
                    classInstanceGc);
    setClassClosure(lua.lua_state(), classTable, "new", classTable, classNew);
    sol::table classMetatable = lua.create_table();
    setClassClosure(lua.lua_state(), classMetatable, "__index", classTable,
                    classMetatableIndex);
    setClassClosure(lua.lua_state(), classMetatable, "__newindex", classTable,
                    classMetatableNewIndex);
    setClassClosure(lua.lua_state(), classMetatable, "__call", classTable,
                    classCall);
    classTable[sol::metatable_key] = classMetatable;
    for (const sol::object& method : ownMethods) {
        registerMethodOwner(lua, classTable, method);
    }
    return classTable;
}

}  // namespace ludork::standard::class_runtime::detail
