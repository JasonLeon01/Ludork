#pragma once

#include <LudorkRuntimeBinding/NativeObjectCodec.hpp>

#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace ludork::runtime::binding {

inline bool isJsonNull(const sol::object& value) {
    if (!value.valid()) {
        return false;
    }
    sol::state_view lua(value.lua_state());
    const sol::object sentinel =
        lua.registry().raw_get<sol::object>("LuaSF.JsonNullSentinel");
    if (isNil(sentinel)) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push();
    sentinel.push();
    const bool result = lua_rawequal(state, -2, -1) != 0;
    lua_pop(state, 2);
    return result;
}

template <typename DynamicValue>
bool dynamicTableIsArray(const sol::table& table, std::size_t& length) {
    const sol::object rawLength = table.raw_get<sol::object>("n");
    const bool hasLength = !isNil(rawLength);
    if (hasLength) {
        if (!rawLength.is<lua_sf::LuaIntegral<std::size_t>>()) {
            return false;
        }
        length = rawLength.as<lua_sf::LuaIntegral<std::size_t>>().value();
    } else {
        length = table.size();
        if (length == 0) {
            return false;
        }
    }
    std::size_t itemCount = 0;
    for (const auto& entry : table) {
        if (entry.first.is<std::string>() &&
            entry.first.as<std::string>() == "n" && hasLength) {
            continue;
        }
        std::int64_t index = 0;
        if (!luaIntegerValue(entry.first, index) || index <= 0 ||
            static_cast<std::size_t>(index) > length) {
            return false;
        }
        ++itemCount;
    }
    if (!hasLength && itemCount != length) {
        return false;
    }
    return true;
}

inline bool dynamicValueHasCycle(
    const sol::object& value, std::unordered_set<const void*>& activeTables,
    std::unordered_set<const void*>& visitedTables) {
    if (!value.is<sol::table>() || luaValueHasMetatable(value)) {
        return false;
    }
    const void* identity = luaValueIdentity(value);
    if (activeTables.contains(identity)) {
        return true;
    }
    if (visitedTables.contains(identity)) {
        return false;
    }
    activeTables.insert(identity);
    const sol::table table = value.as<sol::table>();
    for (const auto& entry : table) {
        if (dynamicValueHasCycle(entry.second, activeTables, visitedTables)) {
            activeTables.erase(identity);
            return true;
        }
    }
    activeTables.erase(identity);
    visitedTables.insert(identity);
    return false;
}

template <typename DynamicValue>
typename DynamicValue::Object makeOpaqueDynamicObject(
    const sol::object& value) {
    using Object = typename DynamicValue::Object;
    using Base = typename Object::element_type;
    static_assert(std::is_polymorphic_v<Base>);
    static_assert(!std::is_abstract_v<Base>);
    return std::make_shared<LuaOpaqueObject<Base>>(value);
}

template <typename DynamicValue>
DynamicValue makeOpaqueDynamicValue(const sol::object& value) {
    using Storage = typename DynamicValue::Storage;
    using Identity = typename DynamicIdentityType<Storage>::Type;
    if constexpr (!std::is_void_v<Identity>) {
        return DynamicValue(readOpaqueIdentity<Identity>(value));
    } else {
        return DynamicValue(makeOpaqueDynamicObject<DynamicValue>(value));
    }
}

template <typename DynamicValue>
bool canReadDynamicValue(const sol::object& value) {
    return value.valid();
}

template <typename DynamicValue>
DynamicValue readAcyclicDynamicValue(const sol::object& value) {
    using Array = typename DynamicValue::Array;
    using Map = typename DynamicValue::Map;
    using Object = typename DynamicValue::Object;
    using Identity =
        typename DynamicIdentityType<typename DynamicValue::Storage>::Type;
    if (isNil(value) || isJsonNull(value)) {
        return DynamicValue();
    }
    if (value.is<DynamicValue>()) {
        return value.as<DynamicValue>();
    }
    if (value.is<bool>()) {
        return DynamicValue(value.as<bool>());
    }
    if (value.get_type() == sol::type::number) {
        std::int64_t integer = 0;
        if (luaIntegerValue(value, integer)) {
            return DynamicValue(integer);
        }
        return DynamicValue(value.as<double>());
    }
    if (value.is<std::string>()) {
        return DynamicValue(value.as<std::string>());
    }
    if (isLuaCompositeValue(value)) {
        return makeOpaqueDynamicValue<DynamicValue>(value);
    }
    Object object;
    if (tryReadSharedPointer(value, object)) {
        return DynamicValue(std::move(object));
    }
    if constexpr (!std::is_void_v<Identity>) {
        Identity identity;
        if (tryReadNativeValue(value, identity)) {
            return DynamicValue(std::move(identity));
        }
    }
    if (!value.is<sol::table>()) {
        return makeOpaqueDynamicValue<DynamicValue>(value);
    }
    if (luaValueHasMetatable(value)) {
        return makeOpaqueDynamicValue<DynamicValue>(value);
    }
    const sol::table table = value.as<sol::table>();
    std::size_t length = 0;
    if (dynamicTableIsArray<DynamicValue>(table, length)) {
        Array result;
        result.reserve(length);
        for (std::size_t index = 1; index <= length; ++index) {
            result.push_back(readAcyclicDynamicValue<DynamicValue>(
                table.raw_get<sol::object>(index)));
        }
        return DynamicValue(std::move(result));
    }
    Map result;
    for (const auto& entry : table) {
        if (!entry.first.is<std::string>()) {
            return makeOpaqueDynamicValue<DynamicValue>(value);
        }
        result.emplace(entry.first.as<std::string>(),
                       readAcyclicDynamicValue<DynamicValue>(entry.second));
    }
    return DynamicValue(std::move(result));
}

template <typename DynamicValue>
DynamicValue readDynamicValue(const sol::object& value) {
    if (value.is<sol::table>() && !luaValueHasMetatable(value)) {
        std::unordered_set<const void*> activeTables;
        std::unordered_set<const void*> visitedTables;
        if (dynamicValueHasCycle(value, activeTables, visitedTables)) {
            return makeOpaqueDynamicValue<DynamicValue>(value);
        }
    }
    return readAcyclicDynamicValue<DynamicValue>(value);
}

template <typename DynamicValue>
sol::object writeDynamicValue(sol::state_view lua, const DynamicValue& value) {
    using Object = typename DynamicValue::Object;
    return value.visit([lua](const auto& item) -> sol::object {
        using Item = LuaValueType<decltype(item)>;
        if constexpr (std::is_same_v<Item, std::monostate>) {
            return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
        } else if constexpr (std::is_same_v<Item, Object>) {
            const ludork::standard::LuaRegistryReference reference =
                ludork::standard::findRuntimeOpaqueValue(lua.lua_state(),
                                                         item.get());
            if (reference) {
                return readLuaRegistryReference(lua, reference);
            }
            return writeLuaValue(lua, item);
        } else {
            return writeLuaValue(lua, item);
        }
    });
}

}  // namespace ludork::runtime::binding
