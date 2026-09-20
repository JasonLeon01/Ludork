#include "LuaObjectSizer.hpp"

#include "ContainerRuntime.hpp"

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace ludork::standard {

namespace {

const void* identity(const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const void* result = lua_topointer(state, -1);
    lua_pop(state, 1);
    return result;
}

std::size_t userdataSize(const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const std::size_t result = lua_rawlen(state, -1);
    lua_pop(state, 1);
    return result;
}

bool isInteger(const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const bool result = lua_isinteger(state, -1) != 0;
    lua_pop(state, 1);
    return result;
}

std::size_t objectSize(const lua_glue::Object& value,
                       std::unordered_set<const void*>& visited) {
    switch (value.get_type()) {
        case lua_glue::Type::None:
        case lua_glue::Type::Nil:
            return 0;
        case lua_glue::Type::Boolean:
            return sizeof(bool);
        case lua_glue::Type::Number:
            return isInteger(value) ? sizeof(lua_Integer) : sizeof(lua_Number);
        case lua_glue::Type::String:
            return sizeof(void*) + value.as<std::string_view>().size() + 1;
        case lua_glue::Type::Table: {
            const void* pointer = identity(value);
            if (!visited.insert(pointer).second) {
                return 0;
            }
            std::size_t result = sizeof(void*);
            const lua_glue::Table table = value.as<lua_glue::Table>();
            for (const auto& entry : table) {
                result += objectSize(entry.first, visited);
                result += objectSize(entry.second, visited);
                result += sizeof(void*) * 2;
            }
            return result;
        }
        case lua_glue::Type::Userdata: {
            const void* pointer = identity(value);
            if (!visited.insert(pointer).second) {
                return 0;
            }
            if (container_runtime::isContainer(value)) {
                std::size_t result =
                    sizeof(void*) +
                    container_runtime::containerStorageSize(value);
                for (const lua_glue::Object& child :
                     container_runtime::containerChildren(value)) {
                    result += objectSize(child, visited);
                }
                return result;
            }
            return sizeof(void*) + userdataSize(value);
        }
        case lua_glue::Type::Function:
        case lua_glue::Type::Thread:
        case lua_glue::Type::LightUserdata:
            return visited.insert(identity(value)).second ? sizeof(void*) : 0;
        default:
            return 0;
    }
}

}  // namespace

std::int64_t luaObjectSize(const lua_glue::Object& value) {
    std::unordered_set<const void*> visited;
    return static_cast<std::int64_t>(objectSize(value, visited));
}

}  // namespace ludork::standard
