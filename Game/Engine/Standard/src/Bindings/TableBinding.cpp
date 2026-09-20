#include <LuaError.hpp>
#include "Bindings.hpp"

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace ludork::standard::binding {

namespace {

std::optional<lua_Integer> tableIndex(const lua_glue::Table& values,
                                      const lua_glue::Object& expected) {
    lua_State* state = values.lua_state();
    for (lua_Integer index = 1;; ++index) {
        const auto value = values.get<lua_glue::Object>(index);
        if (value == lua_glue::nil) {
            return std::nullopt;
        }
        lua_glue::StackGuard stack(state);
        value.push(state);
        expected.push(state);
        if (ludork::standard::compareLuaValues(state, -2, -1, LUA_OPEQ)) {
            return index;
        }
    }
}

bool tableContains(const lua_glue::Table& values,
                   const lua_glue::Object& expected) {
    return tableIndex(values, expected).has_value();
}

lua_glue::Table orderedStringKeys(
    const lua_glue::Table& values,
    const std::optional<lua_glue::Table>& preferredOrder,
    lua_glue::ThisState state) {
    std::unordered_set<std::string> remaining;
    for (const auto& entry : values) {
        if (!entry.first.is<std::string>()) {
            throw std::invalid_argument(
                "orderedStringKeys values must use only string keys");
        }
        remaining.insert(entry.first.as<std::string>());
    }

    std::vector<std::string> ordered;
    ordered.reserve(remaining.size());
    if (preferredOrder.has_value()) {
        const lua_glue::Table preferred = *preferredOrder;
        for (lua_Integer index = 1;; ++index) {
            const lua_glue::Object value =
                preferred.get<lua_glue::Object>(index);
            if (!value.valid() || value.get_type() == lua_glue::Type::Nil) {
                break;
            }
            if (!value.is<std::string>()) {
                throw std::invalid_argument(
                    "orderedStringKeys preferredOrder must contain only "
                    "strings");
            }
            const std::string key = value.as<std::string>();
            if (remaining.erase(key) != 0) {
                ordered.push_back(key);
            }
        }
    }

    std::vector<std::string> extras(remaining.begin(), remaining.end());
    std::sort(extras.begin(), extras.end());
    ordered.insert(ordered.end(), extras.begin(), extras.end());

    lua_glue::StateView lua(state);
    lua_glue::Table result =
        lua.create_table(static_cast<int>(ordered.size()), 0);
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        result.raw_set(index + 1, ordered[index]);
    }
    return result;
}

}  // namespace

void registerTable(lua_glue::StateView lua) {
    const lua_glue::Object rawTable =
        lua.globals().raw_get<lua_glue::Object>("table");
    if (!rawTable.is<lua_glue::Table>()) {
        throw std::runtime_error("Lua table library is not defined");
    }
    lua_glue::Table tableLibrary = rawTable.as<lua_glue::Table>();
    tableLibrary.set_function("contains", &tableContains);
    tableLibrary.set_function("index", &tableIndex);
    tableLibrary.set_function("orderedStringKeys", &orderedStringKeys);
}

}  // namespace ludork::standard::binding
