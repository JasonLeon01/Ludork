#include <LuaError.hpp>
#include "Class/ClassRuntimeInternals.hpp"
#include "Detail/RuntimeState.hpp"

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/TypeQueries.hpp"
#include "Detail/TypedFields.hpp"
#include "Instance/InstanceRuntime.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaGlue/LuaGlue.hpp>

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
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        lua_glue::StateView lua(state);
        const lua_glue::Table classTable = constructorClass(state);
        const lua_glue::Object target =
            lua_glue::Read<lua_glue::Object>(state, 1);
        const lua_glue::Object key = lua_glue::Read<lua_glue::Object>(state, 2);
        const lua_glue::Object disposeMethod =
            instanceDisposeMethod(lua, classTable, key);
        if (disposeMethod.is<lua_glue::Function>()) {
            disposeMethod.push(state);
            return 1;
        }
        const lua_glue::Object getter =
            findAccessor(lua, classTable, protocol::CLASS_GETTERS_FIELD, key);
        if (getter.is<lua_glue::Function>()) {
            getter.push(state);
            target.push(state);
            if (ludork::standard::protectedLuaCall(state, 1, 1) != LUA_OK) {
                throw std::runtime_error(
                    ludork::standard::luaErrorMessage(state, -1));
            }
            return 1;
        }
        if (hasExplicitNilField(lua, target, key)) {
            lua_pushnil(state);
        } else {
            findInClass(lua, classTable, key).push(state);
        }
        return 1;
    });
}

int classInstanceNewIndex(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        lua_glue::StateView lua(state);
        lua_glue::Table classTable = constructorClass(state);
        const lua_glue::Object key = lua_glue::Read<lua_glue::Object>(state, 2);
        const lua_glue::Object setter =
            findAccessor(lua, classTable, protocol::CLASS_SETTERS_FIELD, key);
        if (setter.is<lua_glue::Function>()) {
            setter.push(state);
            lua_pushvalue(state, 1);
            lua_pushvalue(state, 3);
            if (ludork::standard::protectedLuaCall(state, 2, 0) != LUA_OK) {
                throw std::runtime_error(
                    ludork::standard::luaErrorMessage(state, -1));
            }
            clearExplicitNilField(state, 1, 2);
            return 0;
        }
        lua_pushvalue(state, 2);
        lua_pushvalue(state, 3);
        lua_rawset(state, 1);
        clearExplicitNilField(state, 1, 2);
        invalidateClassLookup(lua, classTable);
        const lua_glue::Object value =
            lua_glue::Read<lua_glue::Object>(state, 3);
        if (value.is<lua_glue::Function>()) {
            const lua_glue::Object implementationOwner =
                classTable.raw_get<lua_glue::Object>("_hasImplementationOwner");
            if (implementationOwner.valid() &&
                implementationOwner.get_type() != lua_glue::Type::Nil) {
                classTable.raw_set("_hasImplementationOwner", lua_glue::nil);
            }
        }
        registerMethodOwner(lua, classTable, value);
        return 0;
    });
}

int classMetatableIndex(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        lua_glue::StateView lua(state);
        const lua_glue::Object key = lua_glue::Read<lua_glue::Object>(state, 2);
        if (hasExplicitNilField(state, 1, 2)) {
            lua_pushnil(state);
        } else {
            findInClass(lua, constructorClass(state), key, false).push(state);
        }
        return 1;
    });
}

int classMetatableNewIndex(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        lua_glue::StateView lua(state);
        lua_glue::Table classTable = constructorClass(state);
        const lua_glue::Object value =
            lua_glue::Read<lua_glue::Object>(state, 3);
        lua_pushvalue(state, 2);
        lua_pushvalue(state, 3);
        lua_rawset(state, 1);
        clearExplicitNilField(state, 1, 2);
        invalidateClassLookup(lua, classTable);
        if (value.is<lua_glue::Function>()) {
            const lua_glue::Object implementationOwner =
                classTable.raw_get<lua_glue::Object>("_hasImplementationOwner");
            if (implementationOwner.valid() &&
                implementationOwner.get_type() != lua_glue::Type::Nil) {
                classTable.raw_set("_hasImplementationOwner", lua_glue::nil);
            }
        }
        registerMethodOwner(lua, classTable, value);
        return 0;
    });
}

bool isFinalizedClass(const lua_glue::Table& value) {
    return isClass(value) && tableHasMetatable(value) &&
           value.raw_get<lua_glue::Object>(BASES_FIELD).is<lua_glue::Table>() &&
           value.raw_get<lua_glue::Object>(MRO_FIELD).is<lua_glue::Table>() &&
           value.raw_get<lua_glue::Object>("__index")
               .is<lua_glue::Function>() &&
           value.raw_get<lua_glue::Object>("__newindex")
               .is<lua_glue::Function>() &&
           value.raw_get<lua_glue::Object>("new").is<lua_glue::Function>();
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

void validateClassDefinition(const lua_glue::Table& definition) {
    if (rawBool(definition, protocol::CLASS_MARKER_FIELD)) {
        throw std::invalid_argument("Class definition is already finalized");
    }
    if (tableHasMetatable(definition)) {
        throw std::invalid_argument(
            "Class definition must be a plain table without a metatable");
    }
    for (const char* name : CLASS_RESERVED_FIELDS) {
        const lua_glue::Object value =
            definition.raw_get<lua_glue::Object>(name);
        if (value.valid() && value.get_type() != lua_glue::Type::Nil) {
            throw std::invalid_argument(
                "Class definition contains reserved field '" +
                std::string(name) + "'");
        }
    }
}

lua_glue::Table normalizeClassBases(lua_glue::StateView lua,
                                    const lua_glue::Table& bases) {
    lua_glue::Table result = lua.create_table();
    std::vector<lua_glue::Table> accepted;
    accepted.reserve(bases.size());
    for (std::size_t index = 1; index <= bases.size(); ++index) {
        const lua_glue::Object rawBase = bases.raw_get<lua_glue::Object>(index);
        if (!rawBase.is<lua_glue::Table>()) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        const lua_glue::Table base = rawBase.as<lua_glue::Table>();
        if (!isFinalizedClass(base) && !isNativeType(lua, base)) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        bool duplicate = false;
        for (const lua_glue::Table& existing : accepted) {
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

void setClassClosure(lua_State* state, const lua_glue::Table& target,
                     const char* name, const lua_glue::Table& classTable,
                     lua_CFunction function) {
    target.push(state);
    lua_pushstring(state, name);
    classTable.push(state);
    lua_pushcclosure(state, function, 1);
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

// ── Class finalization
// ────────────────────────────────────────────────────────

lua_glue::Table finalizeClassImpl(lua_glue::Table definition,
                                  const lua_glue::Table& bases) {
    lua_glue::StateView lua(definition.lua_state());
    validateClassDefinition(definition);
    const lua_glue::Table baseList = normalizeClassBases(lua, bases);
    const std::vector<lua_glue::Table> linearization =
        createMro(definition, baseList, MroKind::Runtime);
    for (std::size_t index = 1; index < linearization.size(); ++index) {
        if (isNativeType(lua, linearization[index])) {
            ensureNativeInitializer(lua, linearization[index]);
        }
    }
    std::vector<lua_glue::Object> ownMethods;
    for (const auto& entry : definition) {
        if (entry.second.is<lua_glue::Function>()) {
            ownMethods.push_back(entry.second);
        }
    }
    lua_glue::Table mro = lua.create_table();
    for (const lua_glue::Table& type : linearization) {
        mro.add(type);
    }
    lua_glue::Table classTable = definition;
    classTable.raw_set(protocol::CLASS_MARKER_FIELD, true);
    classTable.raw_set(LOOKUP_VERSION_FIELD, 1);
    classTable.raw_set(BASES_FIELD, baseList);
    if (baseList.size() > 0) {
        classTable.raw_set(protocol::CLASS_BASE_FIELD, baseList[1]);
    }
    classTable.raw_set(MRO_FIELD, mro);
    ensureMroSet(lua, classTable, mro, MRO_SET_FIELD);
    for (const lua_glue::Table& base : tableList(baseList)) {
        registerSubclass(lua, base, classTable);
    }
    setClassClosure(lua.lua_state(), classTable, "__index", classTable,
                    classInstanceIndex);
    setClassClosure(lua.lua_state(), classTable, "__newindex", classTable,
                    classInstanceNewIndex);
    setClassClosure(lua.lua_state(), classTable, "__gc", classTable,
                    classInstanceGc);
    setClassClosure(lua.lua_state(), classTable, "new", classTable, classNew);
    lua_glue::Table classMetatable = lua.create_table();
    setClassClosure(lua.lua_state(), classMetatable, "__index", classTable,
                    classMetatableIndex);
    setClassClosure(lua.lua_state(), classMetatable, "__newindex", classTable,
                    classMetatableNewIndex);
    setClassClosure(lua.lua_state(), classMetatable, "__call", classTable,
                    classCall);
    lua_glue::SetMetatable(classTable, classMetatable);
    for (const lua_glue::Object& method : ownMethods) {
        registerMethodOwner(lua, classTable, method);
    }
    return classTable;
}

}  // namespace ludork::standard::class_runtime::detail
