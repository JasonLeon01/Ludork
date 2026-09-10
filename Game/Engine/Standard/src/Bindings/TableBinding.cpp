#include "Bindings.hpp"

#include <sol2/sol.hpp>

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

sol::optional<lua_Integer> tableIndex(const sol::table& values,
                                      const sol::object& expected) {
    lua_State* state = values.lua_state();
    values.push();
    const int valuesIndex = lua_gettop(state);
    for (lua_Integer index = 1;; ++index) {
        lua_geti(state, valuesIndex, index);
        if (lua_isnil(state, -1)) {
            lua_pop(state, 2);
            return sol::nullopt;
        }
        expected.push();
        const bool equal = lua_compare(state, -2, -1, LUA_OPEQ) != 0;
        lua_pop(state, 2);
        if (equal) {
            lua_pop(state, 1);
            return index;
        }
    }
}

bool tableContains(const sol::table& values, const sol::object& expected) {
    return tableIndex(values, expected).has_value();
}

sol::table orderedStringKeys(const sol::table& values,
                             const sol::optional<sol::table>& preferredOrder,
                             sol::this_state state) {
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
        const sol::table preferred = *preferredOrder;
        for (lua_Integer index = 1;; ++index) {
            const sol::object value = preferred.get<sol::object>(index);
            if (!value.valid() || value.get_type() == sol::type::lua_nil) {
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

    sol::state_view lua(state);
    sol::table result = lua.create_table(static_cast<int>(ordered.size()), 0);
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        result.raw_set(index + 1, ordered[index]);
    }
    return result;
}

}  // namespace

void registerTable(sol::state_view lua) {
    const sol::object rawTable = lua.globals().raw_get<sol::object>("table");
    if (!rawTable.is<sol::table>()) {
        throw std::runtime_error("Lua table library is not defined");
    }
    sol::table tableLibrary = rawTable.as<sol::table>();
    tableLibrary.set_function("contains", &tableContains);
    tableLibrary.set_function("index", &tableIndex);
    tableLibrary.set_function("orderedStringKeys", &orderedStringKeys);
}

}  // namespace ludork::standard::binding
