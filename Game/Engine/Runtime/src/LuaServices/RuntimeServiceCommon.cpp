#include <Runtime/Detail/RuntimeServices.hpp>
#include <ClassRuntimeProtocol.hpp>

#include <ClassServices.hpp>
#include <LuaError.hpp>

extern "C" {
#include <lua.h>
}

#include <climits>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::runtime::detail {

void ensureRuntimeLuaStack(lua_State* state, std::size_t count,
                           const char* context) {
    if (count > static_cast<std::size_t>(INT_MAX)) {
        throw std::length_error(std::string(context) + " count overflow");
    }
    if (count != 0 && lua_checkstack(state, static_cast<int>(count)) == 0) {
        throw std::runtime_error(std::string("Lua stack cannot grow for ") +
                                 context);
    }
}

int invokeRuntimeFunction(lua_State* state, const lua_glue::Object& callable,
                          const std::vector<lua_glue::Object>& arguments,
                          const char* context) {
    const int stackBase = lua_gettop(state);
    try {
        if (arguments.size() > static_cast<std::size_t>(INT_MAX - 1)) {
            throw std::length_error(std::string(context) + " count overflow");
        }
        ensureRuntimeLuaStack(state, arguments.size() + 1, context);
        callable.push(state);
        for (const lua_glue::Object& argument : arguments) {
            argument.push(state);
        }
        const int status = ludork::standard::protectedLuaCall(
            state, static_cast<int>(arguments.size()), LUA_MULTRET);
        ensureRuntimeLuaStack(state, LUA_MINSTACK, "runtime function results");
        if (status != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }
        return lua_gettop(state) - stackBase;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

lua_glue::Object nilObject(lua_glue::StateView lua) {
    return lua_glue::MakeObject(lua, lua_glue::nil);
}

lua_glue::Object protectedIndex(lua_glue::StateView lua,
                                const lua_glue::Object& target,
                                const lua_glue::Object& key) {
    return ludork::standard::class_runtime::protectedGet(lua, target, key);
}

void protectedAssign(lua_glue::StateView lua, const lua_glue::Object& target,
                     const lua_glue::Object& key,
                     const lua_glue::Object& value) {
    ludork::standard::class_runtime::protectedSet(lua, target, key, value);
}

lua_glue::Table createWeakTable(lua_glue::StateView lua, const char* mode) {
    lua_glue::Table result = lua.create_table();
    lua_glue::Table metatable = lua.create_table();
    metatable["__mode"] = mode;
    lua_glue::SetMetatable(result, metatable);
    return result;
}

lua_glue::Table registryTable(lua_glue::StateView lua, const char* key,
                              const char* weakMode) {
    lua_glue::Table registry = lua.registry();
    const lua_glue::Object value = registry.raw_get<lua_glue::Object>(key);
    if (value.is<lua_glue::Table>()) {
        return value.as<lua_glue::Table>();
    }
    lua_glue::Table result = weakMode == nullptr
                                 ? lua.create_table()
                                 : createWeakTable(lua, weakMode);
    registry.raw_set(key, result);
    return result;
}

bool rawBool(const lua_glue::Table& table, const char* name) {
    const lua_glue::Object value = table.raw_get<lua_glue::Object>(name);
    return value.is<bool>() && value.as<bool>();
}

bool isClass(const lua_glue::Table& value) {
    const lua_glue::Object marker = value.raw_get<lua_glue::Object>(
        ludork::standard::class_runtime::protocol::CLASS_MARKER_FIELD);
    return marker.is<bool>() && marker.as<bool>();
}

lua_glue::Table objectMetatable(lua_glue::StateView lua,
                                const lua_glue::Object& value) {
    lua_State* state = lua.lua_state();
    value.push(state);
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return lua.create_table();
    }
    lua_glue::Table result = lua_glue::Read<lua_glue::Table>(state, -1);
    lua_pop(state, 2);
    return result;
}

bool isNativeType(lua_glue::StateView lua, const lua_glue::Table& value) {
    return !isClass(value) &&
           objectMetatable(lua, lua_glue::MakeObject(lua, value))
               .raw_get<lua_glue::Object>(
                   ludork::standard::class_runtime::protocol::CLASS_TYPE_FIELD)
               .is<lua_glue::Table>();
}

bool isInstance(lua_glue::ThisState state, const lua_glue::Object& value,
                const lua_glue::Table& targetClass) {
    return ludork::standard::class_runtime::isInstanceOf(
        lua_glue::StateView(state), value, targetClass);
}

bool isSubclass(lua_glue::ThisState state, const lua_glue::Table& value,
                const lua_glue::Table& targetClass) {
    return ludork::standard::class_runtime::isSubclassOf(
        lua_glue::StateView(state), value, targetClass);
}

lua_glue::Object classType(lua_glue::ThisState state,
                           const lua_glue::Object& value) {
    return ludork::standard::class_runtime::typeOf(lua_glue::StateView(state),
                                                   value);
}

bool rawEqual(lua_glue::StateView lua, const lua_glue::Object& left,
              const lua_glue::Object& right) {
    static_cast<void>(lua);
    return ludork::standard::class_runtime::rawEqual(left, right);
}

lua_glue::Object checkedResult(lua_glue::StateView lua,
                               lua_glue::CallResult& result) {
    if (!result.valid()) {
        const std::string error = result.error();
        throw std::runtime_error(error.c_str());
    }
    return result.return_count() == 0 ? nilObject(lua)
                                      : result.get<lua_glue::Object>();
}

lua_glue::Table requireLuaTable(lua_glue::StateView lua,
                                const char* moduleName) {
    const lua_glue::Object loaded =
        ludork::standard::class_runtime::requireModule(lua, moduleName);
    if (!loaded.is<lua_glue::Table>()) {
        throw std::runtime_error(
            std::string("Lua module did not return a table: ") + moduleName);
    }
    return loaded.as<lua_glue::Table>();
}

bool luaBoolean(const lua_glue::Object& value) {
    return value.is<bool>() && value.as<bool>();
}

std::vector<lua_glue::Table> runtimeClassMro(
    lua_glue::StateView lua, const lua_glue::Table& classTable) {
    std::vector<lua_glue::Table> result;
    const lua_glue::Table mro = ludork::standard::class_runtime::getMroCopy(
        lua, lua_glue::MakeObject(lua, classTable));
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        const lua_glue::Object value = mro.raw_get<lua_glue::Object>(index);
        if (value.is<lua_glue::Table>()) {
            result.push_back(value.as<lua_glue::Table>());
        }
    }
    if (result.empty()) {
        result.push_back(classTable);
    }
    return result;
}

lua_glue::Object runtimeIndex(lua_glue::StateView lua,
                              const lua_glue::Object& target,
                              const lua_glue::Object& key, bool raw) {
    if (raw) {
        return ludork::standard::class_runtime::rawGetOwnField(lua, target,
                                                               key);
    }
    if (!raw) {
        return protectedIndex(lua, target, key);
    }
    return nilObject(lua);
}

void runtimeAssign(lua_glue::StateView lua, const lua_glue::Object& target,
                   const lua_glue::Object& key, const lua_glue::Object& value,
                   bool raw) {
    if (raw && !target.is<lua_glue::Table>()) {
        throw std::invalid_argument("Raw assignment requires a table");
    }
    if (!raw) {
        protectedAssign(lua, target, key, value);
        return;
    }
    lua_State* state = lua.lua_state();
    target.push(state);
    key.push(state);
    value.push(state);
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

std::vector<lua_glue::Object> runtimeKeys(lua_glue::StateView lua,
                                          const lua_glue::Object& target,
                                          bool raw) {
    std::vector<lua_glue::Object> keys;
    if (raw) {
        const lua_glue::Table ownKeys =
            ludork::standard::class_runtime::getOwnKeys(lua, target);
        keys.reserve(ownKeys.size());
        for (std::size_t index = 1; index <= ownKeys.size(); ++index) {
            keys.push_back(ownKeys.raw_get<lua_glue::Object>(index));
        }
        return keys;
    }

    const lua_glue::Object rawPairs =
        lua.globals().raw_get<lua_glue::Object>("pairs");
    if (!rawPairs.is<lua_glue::Function>()) {
        throw std::runtime_error("Lua pairs function is not defined");
    }
    lua_glue::Function pairs = rawPairs.as<lua_glue::Function>();
    lua_glue::CallResult initialized = pairs(target);
    if (!initialized.valid()) {
        const std::string error = initialized.error();
        throw std::runtime_error(error.c_str());
    }
    if (initialized.return_count() < 3) {
        return keys;
    }
    const lua_glue::Object rawIterator = initialized.get<lua_glue::Object>(0);
    if (!rawIterator.is<lua_glue::Function>()) {
        return keys;
    }
    lua_glue::Function iterator = rawIterator.as<lua_glue::Function>();
    const lua_glue::Object iteratorState = initialized.get<lua_glue::Object>(1);
    lua_glue::Object control = initialized.get<lua_glue::Object>(2);
    for (;;) {
        lua_glue::CallResult next = iterator(iteratorState, control);
        if (!next.valid()) {
            const std::string error = next.error();
            throw std::runtime_error(error.c_str());
        }
        if (next.return_count() == 0) {
            break;
        }
        lua_glue::Object key = next.get<lua_glue::Object>(0);
        if (!key.valid() || key.get_type() == lua_glue::Type::Nil) {
            break;
        }
        keys.push_back(key);
        control = key;
    }
    return keys;
}

}  // namespace ludork::runtime::detail
