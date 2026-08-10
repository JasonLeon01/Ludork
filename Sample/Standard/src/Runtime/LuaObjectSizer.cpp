#include "LuaObjectSizer.hpp"

#include "ContainerRuntime.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace ludork::standard {

namespace {

const void* identity(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const void* result = lua_topointer(state, -1);
    lua_pop(state, 1);
    return result;
}

std::size_t userdataSize(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const std::size_t result = lua_rawlen(state, -1);
    lua_pop(state, 1);
    return result;
}

bool isInteger(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const bool result = lua_isinteger(state, -1) != 0;
    lua_pop(state, 1);
    return result;
}

std::size_t objectSize(const sol::object& value,
                       std::unordered_set<const void*>& visited) {
    switch (value.get_type()) {
        case sol::type::none:
        case sol::type::lua_nil:
            return 0;
        case sol::type::boolean:
            return sizeof(bool);
        case sol::type::number:
            return isInteger(value) ? sizeof(lua_Integer) : sizeof(lua_Number);
        case sol::type::string:
            return sizeof(void*) + value.as<sol::string_view>().size() + 1;
        case sol::type::table: {
            const void* pointer = identity(value);
            if (!visited.insert(pointer).second) {
                return 0;
            }
            std::size_t result = sizeof(void*);
            const sol::table table = value.as<sol::table>();
            for (const auto& entry : table) {
                result += objectSize(entry.first, visited);
                result += objectSize(entry.second, visited);
                result += sizeof(void*) * 2;
            }
            return result;
        }
        case sol::type::userdata: {
            const void* pointer = identity(value);
            if (!visited.insert(pointer).second) {
                return 0;
            }
            if (container_runtime::isContainer(value)) {
                std::size_t result =
                    sizeof(void*) +
                    container_runtime::containerStorageSize(value);
                for (const sol::object& child :
                     container_runtime::containerChildren(value)) {
                    result += objectSize(child, visited);
                }
                return result;
            }
            return sizeof(void*) + userdataSize(value);
        }
        case sol::type::function:
        case sol::type::thread:
        case sol::type::lightuserdata:
            return visited.insert(identity(value)).second ? sizeof(void*) : 0;
        default:
            return 0;
    }
}

}  // namespace

std::int64_t luaObjectSize(const sol::object& value) {
    std::unordered_set<const void*> visited;
    return static_cast<std::int64_t>(objectSize(value, visited));
}

}  // namespace ludork::standard
