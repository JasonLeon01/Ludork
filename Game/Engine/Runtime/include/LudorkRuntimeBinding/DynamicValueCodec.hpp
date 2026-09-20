#pragma once

#include <Cast.hpp>
#include <JsonRuntimeProtocol.hpp>

#include <LudorkRuntimeBinding/NativeObjectCodec.hpp>

#include <cstdint>
#include <memory>
#include <limits>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace ludork::runtime::binding {

inline bool isJsonNull(const lua_glue::Object& value) {
    if (!value.valid()) {
        return false;
    }
    lua_glue::StateView lua(value.lua_state());
    const lua_glue::Object sentinel = lua.registry().raw_get<lua_glue::Object>(
        ludork::standard::json_runtime::protocol::JSON_NULL_KEY);
    if (isNil(sentinel)) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push(state);
    sentinel.push(state);
    const bool result = lua_rawequal(state, -2, -1) != 0;
    lua_pop(state, 2);
    return result;
}

template <typename DynamicValue>
bool dynamicTableIsArray(const lua_glue::Table& table, std::size_t& length) {
    const lua_glue::Object rawLength = table.raw_get<lua_glue::Object>("n");
    const bool hasLength = !isNil(rawLength);
    if (hasLength) {
        if (!rawLength.is<std::size_t>()) {
            return false;
        }
        length = rawLength.as<std::size_t>();
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
    const lua_glue::Object& value,
    std::unordered_set<const void*>& activeTables,
    std::unordered_set<const void*>& visitedTables) {
    if (!value.is<lua_glue::Table>() || luaValueHasMetatable(value)) {
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
    const lua_glue::Table table = value.as<lua_glue::Table>();
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
    const lua_glue::Object& value) {
    using Object = typename DynamicValue::Object;
    using Base = typename Object::element_type;
    static_assert(std::is_polymorphic_v<Base>);
    static_assert(!std::is_abstract_v<Base>);
    return std::make_shared<LuaOpaqueObject<Base>>(value);
}

template <typename DynamicValue>
DynamicValue makeOpaqueDynamicValue(const lua_glue::Object& value) {
    using Identity = typename DynamicValue::Identity;
    if constexpr (!std::is_void_v<Identity>) {
        return DynamicValue(readOpaqueIdentity<Identity>(value));
    } else {
        return DynamicValue(makeOpaqueDynamicObject<DynamicValue>(value));
    }
}

template <typename DynamicValue>
bool canReadDynamicValue(const lua_glue::Object& value) {
    return value.valid();
}

template <typename DynamicValue>
DynamicValue readAcyclicDynamicValue(const lua_glue::Object& value) {
    using Array = typename DynamicValue::Array;
    using Map = typename DynamicValue::Map;
    using Object = typename DynamicValue::Object;
    using Identity = typename DynamicValue::Identity;
    if (isNil(value) || isJsonNull(value)) {
        return DynamicValue();
    }
    if (value.is<DynamicValue>()) {
        return value.as<DynamicValue>();
    }
    if (value.is<bool>()) {
        return DynamicValue(value.as<bool>());
    }
    if (value.get_type() == lua_glue::Type::Number) {
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
    if (!value.is<lua_glue::Table>()) {
        return makeOpaqueDynamicValue<DynamicValue>(value);
    }
    if (luaValueHasMetatable(value)) {
        return makeOpaqueDynamicValue<DynamicValue>(value);
    }
    const lua_glue::Table table = value.as<lua_glue::Table>();
    std::size_t length = 0;
    if (dynamicTableIsArray<DynamicValue>(table, length)) {
        Array result;
        result.reserve(length);
        for (std::size_t index = 1; index <= length; ++index) {
            result.push_back(readAcyclicDynamicValue<DynamicValue>(
                table.raw_get<lua_glue::Object>(index)));
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
DynamicValue readDynamicValue(const lua_glue::Object& value) {
    if (value.is<lua_glue::Table>() && !luaValueHasMetatable(value)) {
        std::unordered_set<const void*> activeTables;
        std::unordered_set<const void*> visitedTables;
        if (dynamicValueHasCycle(value, activeTables, visitedTables)) {
            return makeOpaqueDynamicValue<DynamicValue>(value);
        }
    }
    return readAcyclicDynamicValue<DynamicValue>(value);
}

template <typename DynamicValue>
lua_glue::Object writeDynamicValue(lua_glue::StateView lua,
                                   const DynamicValue& value) {
    using Object = typename DynamicValue::Object;
    const auto write =
        [lua](auto&& recurse,
              typename DynamicValue::View view) -> lua_glue::Object {
        return view.visit([&](const auto& item) -> lua_glue::Object {
            using Item = LuaValueType<decltype(item)>;
            if constexpr (std::is_same_v<Item,
                                         typename DynamicValue::ArrayView>) {
                if (item.size() >
                    static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                    throw std::length_error("Runtime array size overflow");
                }
                lua_glue::Table table =
                    lua_glue::StateView(lua.lua_state())
                        .create_table(static_cast<int>(item.size()), 1);
                table.raw_set("n", item.size());
                std::size_t index = 1;
                for (const auto value : item) {
                    table.raw_set(index++, recurse(recurse, value));
                }
                return lua_glue::MakeObject(lua, table);
            } else if constexpr (std::is_same_v<
                                     Item, typename DynamicValue::MapView>) {
                if (item.size() >
                    static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                    throw std::length_error("Runtime map size overflow");
                }
                lua_glue::Table table =
                    lua_glue::StateView(lua.lua_state())
                        .create_table(0, static_cast<int>(item.size()));
                for (const auto& [key, value] : item) {
                    table.raw_set(key, recurse(recurse, value));
                }
                return lua_glue::MakeObject(lua, table);
            } else if constexpr (std::is_same_v<Item, std::monostate>) {
                return lua_glue::MakeObject(lua, lua_glue::nil);
            } else if constexpr (std::is_same_v<Item, Object>) {
                if (const auto* reference =
                        ludork::Cast<const LuaRegistryReferenceOwner>(
                            item.get())) {
                    return readLuaRegistryReference(
                        lua, reference->registryReference());
                }
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
    };
    return write(write, value.view());
}

}  // namespace ludork::runtime::binding
