#pragma once

#include <JsonRuntimeProtocol.hpp>

#include <LudorkRuntimeBinding/ValueCodec.hpp>

#include <cmath>
#include <unordered_set>

namespace ludork::runtime::binding {

inline bool isPureDataNull(const sol::object& value) {
    if (isNil(value)) {
        return true;
    }
    sol::state_view lua(value.lua_state());
    const sol::object sentinel = lua.registry().raw_get<sol::object>(
        ludork::standard::json_runtime::protocol::JSON_NULL_KEY);
    if (isNil(sentinel)) {
        return false;
    }
    auto pushedValue = sol::stack::push_pop(value);
    auto pushedSentinel = sol::stack::push_pop(sentinel);
    return lua_rawequal(value.lua_state(), pushedValue.index_of(value),
                        pushedSentinel.index_of(sentinel)) != 0;
}

template <typename Data>
Data readPureDataValueImpl(const sol::object& value,
                           std::unordered_set<const void*>& active) {
    if (isPureDataNull(value)) {
        return {};
    }
    switch (value.get_type()) {
        case sol::type::boolean:
            return Data(value.as<bool>());
        case sol::type::number: {
            auto pushed = sol::stack::push_pop(value);
            if (lua_isinteger(value.lua_state(), pushed.index_of(value))) {
                return Data(static_cast<std::int64_t>(
                    lua_tointeger(value.lua_state(), pushed.index_of(value))));
            }
            const double number = value.as<double>();
            if (!std::isfinite(number)) {
                throw std::invalid_argument(
                    "Pure data requires finite numbers");
            }
            return Data(number);
        }
        case sol::type::string:
            return Data(value.as<std::string>());
        case sol::type::table:
            break;
        default:
            throw std::invalid_argument(
                "Pure data cannot contain objects or functions");
    }
    auto pushed = sol::stack::push_pop(value);
    lua_State* state = value.lua_state();
    const int tableIndex = pushed.index_of(value);
    bool jsonArray = false;
    if (lua_getmetatable(state, tableIndex)) {
        sol::state_view lua(state);
        const sol::object metatable = sol::stack::get<sol::object>(state, -1);
        lua_pop(state, 1);
        for (const char* key : {ludork::standard::json_runtime::protocol::
                                    JSON_ARRAY_METATABLE_KEY,
                                ludork::standard::json_runtime::protocol::
                                    JSON_EMPTY_ARRAY_METATABLE_KEY}) {
            const sol::object known = lua.registry().raw_get<sol::object>(key);
            auto pushedMetatable = sol::stack::push_pop(metatable);
            auto pushedKnown = sol::stack::push_pop(known);
            if (!isNil(known) &&
                lua_rawequal(state, pushedMetatable.index_of(metatable),
                             pushedKnown.index_of(known))) {
                jsonArray = true;
            }
        }
        if (!jsonArray) {
            throw std::invalid_argument(
                "Pure data cannot contain object metatables");
        }
    }
    const void* identity = lua_topointer(state, tableIndex);
    if (!active.insert(identity).second) {
        throw std::invalid_argument("Pure data cannot contain cycles");
    }
    try {
        const sol::table table = value.as<sol::table>();
        const sol::object explicitLength = table.raw_get<sol::object>("n");
        bool hasOtherStringKey = false;
        for (const auto& entry : table) {
            if (entry.first.get_type() == sol::type::string &&
                entry.first.as<std::string>() != "n") {
                hasOtherStringKey = true;
            }
        }
        const bool hasLength = !isNil(explicitLength) && !hasOtherStringKey;
        std::size_t length = table.size();
        if (hasLength) {
            if (!explicitLength.is<lua_sf::LuaIntegral<std::size_t>>()) {
                throw std::invalid_argument(
                    "Pure data array n must be a non-negative integer");
            }
            length =
                explicitLength.as<lua_sf::LuaIntegral<std::size_t>>().value();
        }
        bool array = jsonArray || hasLength || length != 0;
        std::size_t count = 0;
        for (const auto& entry : table) {
            if (hasLength && entry.first.is<std::string>() &&
                entry.first.as<std::string>() == "n") {
                continue;
            }
            std::int64_t index = 0;
            if (!luaIntegerValue(entry.first, index) || index <= 0 ||
                static_cast<std::uint64_t>(index) > length) {
                array = false;
            }
            ++count;
        }
        if (((hasLength || jsonArray) && !array) ||
            (jsonArray && !hasLength && count != length)) {
            throw std::invalid_argument("Pure data array has an invalid key");
        }
        if (array && (hasLength || count == length)) {
            typename Data::Array result;
            result.reserve(length);
            for (std::size_t index = 1; index <= length; ++index) {
                result.push_back(readPureDataValueImpl<Data>(
                    table.raw_get<sol::object>(index), active));
            }
            active.erase(identity);
            return Data(std::move(result));
        }
        typename Data::Map result;
        for (const auto& entry : table) {
            if (entry.first.get_type() != sol::type::string) {
                throw std::invalid_argument(
                    "Pure data maps require string keys");
            }
            result.emplace(entry.first.as<std::string>(),
                           readPureDataValueImpl<Data>(entry.second, active));
        }
        active.erase(identity);
        return Data(std::move(result));
    } catch (...) {
        active.erase(identity);
        throw;
    }
}

template <typename Data>
Data readPureDataValue(const sol::object& value) {
    std::unordered_set<const void*> active;
    return readPureDataValueImpl<Data>(value, active);
}

template <typename Data>
bool canReadPureDataValue(const sol::object& value) {
    try {
        static_cast<void>(readPureDataValue<Data>(value));
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

template <typename Data>
sol::object writePureDataValue(sol::state_view lua, const Data& value) {
    if (value.isNil()) {
        return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
    }
    if (const bool* item = value.template getIf<bool>()) {
        return writeLuaValue(lua, *item);
    }
    if (const std::int64_t* item = value.template getIf<std::int64_t>()) {
        return writeLuaValue(lua, *item);
    }
    if (const double* item = value.template getIf<double>()) {
        return writeLuaValue(lua, *item);
    }
    if (const std::string* item = value.template getIf<std::string>()) {
        return writeLuaValue(lua, *item);
    }
    if (const auto* items = value.template getIf<typename Data::Array>()) {
        sol::table result = lua.create_table();
        result.raw_set("n", items->size());
        for (std::size_t index = 0; index < items->size(); ++index) {
            result.raw_set(index + 1, writePureDataValue(lua, (*items)[index]));
        }
        return sol::make_object(lua, result);
    }
    sol::table result = lua.create_table();
    const auto* items = value.template getIf<typename Data::Map>();
    if (items == nullptr) {
        throw std::invalid_argument("Unknown pure data value");
    }
    for (const auto& [name, item] : *items) {
        if (item.isNil()) {
            const sol::object null = lua.registry().raw_get<sol::object>(
                ludork::standard::json_runtime::protocol::JSON_NULL_KEY);
            if (isNil(null)) {
                throw std::runtime_error("JSON null sentinel is unavailable");
            }
            result.raw_set(name, null);
        } else {
            result.raw_set(name, writePureDataValue(lua, item));
        }
    }
    return sol::make_object(lua, result);
}

}  // namespace ludork::runtime::binding
