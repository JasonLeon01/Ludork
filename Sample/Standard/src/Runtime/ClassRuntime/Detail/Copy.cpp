#include "Detail/CopyRuntime.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassServices.hpp>

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ludork::standard::class_runtime::detail {

bool isAtomicClassTable(sol::state_view lua, const sol::table& value);
sol::object copyNativeValue(sol::state_view lua, const sol::object& value);

}  // namespace ludork::standard::class_runtime::detail

namespace {

using namespace ludork::standard::class_runtime::detail;

sol::object checkedResult(sol::state_view lua,
                          sol::protected_function_result& result) {
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    return result.return_count() == 0 ? nilObject(lua)
                                      : result.get<sol::object>();
}

void copyTableMetatable(const sol::table& source, const sol::table& target) {
    lua_State* state = source.lua_state();
    source.push();
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return;
    }
    target.push();
    lua_pushvalue(state, -2);
    lua_setmetatable(state, -2);
    lua_pop(state, 3);
}

sol::object deepCopyImpl(sol::state_view lua, const sol::object& value,
                         std::unordered_map<const void*, sol::object>& visited);

struct NativeDeepCopyContext {
    sol::state_view lua;
    std::unordered_map<const void*, sol::object>* visited = nullptr;
};

sol::object deepCopyNativeChild(void* rawContext, const sol::object& value) {
    auto* context = static_cast<NativeDeepCopyContext*>(rawContext);
    if (context == nullptr || context->visited == nullptr) {
        throw std::runtime_error("Native deep-copy context is unavailable");
    }
    return deepCopyImpl(context->lua, value, *context->visited);
}

sol::object deepCopyNativeValue(
    sol::state_view lua, const sol::object& value, const void* identity,
    std::unordered_map<const void*, sol::object>& visited) {
    const sol::object rawType = nativeTypeOf(lua, value);
    if (rawType.get_type() != sol::type::table) {
        visited.emplace(identity, value);
        return value;
    }
    const auto protocol = findNativeDeepCopyProtocol(lua, rawType);
    if (!protocol.has_value()) {
        const sol::object result = copyNativeValue(lua, value);
        visited.emplace(identity, result);
        return result;
    }
    NativeDeepCopyContext context{lua, &visited};
    if (protocol->mode ==
        ludork::standard::class_runtime::NativeDeepCopyMode::TwoPhase) {
        if (protocol->create == nullptr || protocol->populate == nullptr) {
            throw std::runtime_error(
                "Native two-phase deep-copy protocol is incomplete");
        }
        const sol::object result = protocol->create(lua, value);
        visited.emplace(identity, result);
        protocol->populate(lua, value, result, &deepCopyNativeChild, &context);
        return result;
    }
    if (protocol->build == nullptr) {
        throw std::runtime_error(
            "Native deferred deep-copy protocol is incomplete");
    }
    const sol::object result =
        protocol->build(lua, value, &deepCopyNativeChild, &context);
    const auto existing = visited.find(identity);
    if (existing != visited.end()) {
        return existing->second;
    }
    visited.emplace(identity, result);
    return result;
}

sol::object deepCopyImpl(
    sol::state_view lua, const sol::object& value,
    std::unordered_map<const void*, sol::object>& visited) {
    if (value.get_type() != sol::type::table) {
        if (value.get_type() != sol::type::userdata) {
            return value;
        }
        lua_State* state = lua.lua_state();
        value.push();
        const void* identity = lua_topointer(state, -1);
        lua_pop(state, 1);
        const auto existing = visited.find(identity);
        if (existing != visited.end()) {
            return existing->second;
        }
        return deepCopyNativeValue(lua, value, identity, visited);
    }
    const sol::table source = value.as<sol::table>();
    if (isAtomicClassTable(lua, source)) {
        return value;
    }
    lua_State* state = lua.lua_state();
    source.push();
    const void* identity = lua_topointer(state, -1);
    lua_pop(state, 1);
    const auto existing = visited.find(identity);
    if (existing != visited.end()) {
        return existing->second;
    }
    sol::table result = lua.create_table();
    visited.emplace(identity, sol::make_object(lua, result));
    for (const auto& entry : source) {
        result.raw_set(entry.first, deepCopyImpl(lua, entry.second, visited));
    }
    copyTableMetatable(source, result);
    return sol::make_object(lua, result);
}

sol::object clonePlainDataImpl(sol::state_view lua, const sol::object& value) {
    if (value.get_type() != sol::type::table) {
        return value;
    }
    const sol::table source = value.as<sol::table>();
    if (isAtomicClassTable(lua, source)) {
        return value;
    }
    sol::table result = lua.create_table();
    for (const auto& entry : source) {
        result.raw_set(entry.first, clonePlainDataImpl(lua, entry.second));
    }
    copyTableMetatable(source, result);
    return sol::make_object(lua, result);
}

}  // namespace

namespace ludork::standard::class_runtime::detail {

bool isAtomicClassTable(sol::state_view lua, const sol::table& value) {
    return isClass(value) || isNativeType(lua, value);
}

sol::object copyNativeValue(sol::state_view lua, const sol::object& value) {
    if (value.get_type() != sol::type::userdata) {
        return value;
    }
    const sol::object rawType = nativeTypeOf(lua, value);
    if (!rawType.is<sol::table>()) {
        return value;
    }
    const sol::object rawCopy = protectedIndex(
        lua, rawType, sol::make_object(lua, std::string(NATIVE_COPY_FIELD)));
    if (!rawCopy.is<sol::protected_function>()) {
        return value;
    }
    sol::protected_function copy = rawCopy.as<sol::protected_function>();
    sol::protected_function_result result = copy(value);
    return checkedResult(lua, result);
}

sol::object shallowCopyImpl(sol::state_view lua, const sol::object& value) {
    if (value.get_type() != sol::type::table) {
        return copyNativeValue(lua, value);
    }
    const sol::table source = value.as<sol::table>();
    if (isAtomicClassTable(lua, source)) {
        return value;
    }
    sol::table result = lua.create_table();
    for (const auto& entry : source) {
        result.raw_set(entry.first, entry.second);
    }
    copyTableMetatable(source, result);
    return sol::make_object(lua, result);
}

sol::object deepCopyImpl(
    sol::state_view lua, const sol::object& value,
    std::unordered_map<const void*, sol::object>& visited) {
    return ::deepCopyImpl(lua, value, visited);
}

sol::object deepCopyImpl(sol::state_view lua, const sol::object& value) {
    std::unordered_map<const void*, sol::object> visited;
    return ::deepCopyImpl(lua, value, visited);
}

}  // namespace ludork::standard::class_runtime::detail

namespace ludork::standard::class_runtime {

sol::object clonePlainData(sol::state_view lua, const sol::object& value) {
    return ::clonePlainDataImpl(lua, value);
}

sol::object shallowCopy(sol::state_view lua, const sol::object& value) {
    return detail::shallowCopyImpl(lua, value);
}

sol::object deepCopy(sol::state_view lua, const sol::object& value) {
    return detail::deepCopyImpl(lua, value);
}

void registerNativeDeepCopyProtocol(sol::state_view lua,
                                    const sol::table& nativeType,
                                    const NativeDeepCopyProtocol& protocol) {
    if (protocol.mode == NativeDeepCopyMode::TwoPhase) {
        if (protocol.create == nullptr || protocol.populate == nullptr ||
            protocol.build != nullptr) {
            throw std::invalid_argument(
                "Invalid native two-phase deep-copy protocol");
        }
    } else if (protocol.build == nullptr || protocol.create != nullptr ||
               protocol.populate != nullptr) {
        throw std::invalid_argument(
            "Invalid native deferred deep-copy protocol");
    }
    lua_State* state = lua.lua_state();
    sol::table protocols = detail::nativeDeepCopyProtocols(lua);
    protocols.push();
    nativeType.push();
    void* storage = lua_newuserdatauv(state, sizeof(NativeDeepCopyProtocol), 0);
    new (storage) NativeDeepCopyProtocol(protocol);
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

}  // namespace ludork::standard::class_runtime
