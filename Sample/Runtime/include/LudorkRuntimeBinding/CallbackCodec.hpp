#pragma once

#include <LudorkRuntimeBinding/ValueCodec.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace ludork::runtime::binding {

struct LuaNativeCodecPolicy {};

template <typename NativeCallable, typename Codec, bool AllowNil>
struct LuaSfCallbackCodecPolicy {};

template <typename Item>
struct LuaSequenceCodecPolicy {};

template <typename Key, typename Value>
struct LuaMapCodecPolicy {};

template <typename Item>
struct LuaOptionalCodecPolicy {};

template <typename... Alternatives>
struct LuaVariantCodecPolicy {};

template <typename First, typename Second>
struct LuaPairCodecPolicy {};

template <typename... Items>
struct LuaTupleCodecPolicy {};

template <typename Native, typename Policy>
struct LuaCodecAdapter;

template <typename Native>
struct LuaCodecAdapter<Native, LuaNativeCodecPolicy> {
    static bool canRead(const sol::object& value) {
        return canReadLuaValue<Native>(value);
    }

    static Native read(const sol::object& value, std::string_view) {
        return readLuaValue<Native>(value);
    }

    static sol::object write(sol::state_view lua, const Native& value,
                             std::string_view) {
        return writeLuaValue(lua, value);
    }
};

template <typename Native, typename Expected, typename Codec, bool AllowNil>
struct LuaCodecAdapter<Native,
                       LuaSfCallbackCodecPolicy<Expected, Codec, AllowNil>> {
    static_assert(std::is_same_v<Native, Expected>);

    static bool canRead(const sol::object& value) {
        return (AllowNil && isNil(value)) ||
               value.is<sol::protected_function>();
    }

    static Native read(const sol::object& value, std::string_view label) {
        if (isNil(value)) {
            if constexpr (AllowNil) {
                return Native{};
            }
            throw std::invalid_argument(std::string(label) +
                                        " does not allow nil");
        }
        if (!value.is<sol::protected_function>()) {
            throw std::invalid_argument(std::string("expected ") +
                                        std::string(label));
        }
        return lua_sf::callback::from_object<Expected, Codec>(value, label);
    }

    static sol::object write(sol::state_view lua, const Native& value,
                             std::string_view label) {
        if (!value) {
            if constexpr (AllowNil) {
                return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
            }
            throw std::invalid_argument(std::string(label) +
                                        " does not allow nil");
        }
        return lua_sf::callback::to_object<Expected, Codec>(lua, value, label);
    }
};

template <typename Sequence, typename ItemPolicy>
struct LuaCodecAdapter<Sequence, LuaSequenceCodecPolicy<ItemPolicy>> {
    using Item = typename Sequence::value_type;

    static_assert(IsVector<Sequence>::value || IsArray<Sequence>::value);

    static bool canRead(const sol::object& value) {
        if (!value.is<sol::table>()) {
            return false;
        }
        const sol::table table = value.as<sol::table>();
        std::size_t length = 0;
        if (!trySequenceLength(table, length)) {
            return false;
        }
        if constexpr (IsArray<Sequence>::value) {
            if (length != std::tuple_size_v<Sequence>) {
                return false;
            }
        }
        for (std::size_t index = 1; index <= length; ++index) {
            if (!LuaCodecAdapter<Item, ItemPolicy>::canRead(
                    table.raw_get<sol::object>(index))) {
                return false;
            }
        }
        return true;
    }

    static Sequence read(const sol::object& value, std::string_view label) {
        if (!value.is<sol::table>()) {
            throw std::invalid_argument("expected a Lua sequence table");
        }
        const sol::table table = value.as<sol::table>();
        std::size_t length = 0;
        if (!trySequenceLength(table, length)) {
            throw std::invalid_argument(
                "Lua sequence n must be a non-negative integer");
        }
        if constexpr (IsArray<Sequence>::value) {
            if (length != std::tuple_size_v<Sequence>) {
                throw std::invalid_argument(
                    "Lua sequence length does not match the fixed C++ size");
            }
        }
        Sequence result{};
        if constexpr (IsVector<Sequence>::value) {
            result.reserve(length);
        }
        for (std::size_t index = 1; index <= length; ++index) {
            try {
                Item item = LuaCodecAdapter<Item, ItemPolicy>::read(
                    table.raw_get<sol::object>(index), label);
                if constexpr (IsVector<Sequence>::value) {
                    result.push_back(std::move(item));
                } else {
                    result[index - 1] = std::move(item);
                }
            } catch (const std::exception& error) {
                throw std::invalid_argument("Lua sequence element at index " +
                                            std::to_string(index) + ": " +
                                            error.what());
            }
        }
        return result;
    }

    static sol::object write(sol::state_view lua, const Sequence& value,
                             std::string_view label) {
        sol::table table = lua.create_table(static_cast<int>(value.size()), 1);
        table.raw_set("n", value.size());
        std::size_t index = 1;
        for (const auto& rawItem : value) {
            sol::object output;
            if constexpr (std::is_same_v<Item, bool>) {
                const bool item = static_cast<bool>(rawItem);
                output =
                    LuaCodecAdapter<Item, ItemPolicy>::write(lua, item, label);
            } else {
                output = LuaCodecAdapter<Item, ItemPolicy>::write(lua, rawItem,
                                                                  label);
            }
            if (!isNil(output)) {
                table.raw_set(index, output);
            }
            ++index;
        }
        return sol::make_object(lua, table);
    }
};

template <typename Map, typename KeyPolicy, typename ItemPolicy>
struct LuaCodecAdapter<Map, LuaMapCodecPolicy<KeyPolicy, ItemPolicy>> {
    using Key = typename Map::key_type;
    using Item = typename Map::mapped_type;

    static_assert(IsMap<Map>::value);

    static bool canRead(const sol::object& value) {
        if (!value.is<sol::table>()) {
            return false;
        }
        const sol::table table = value.as<sol::table>();
        for (const auto& entry : table) {
            if (!LuaCodecAdapter<Key, KeyPolicy>::canRead(entry.first) ||
                !LuaCodecAdapter<Item, ItemPolicy>::canRead(entry.second)) {
                return false;
            }
        }
        return true;
    }

    static Map read(const sol::object& value, std::string_view label) {
        if (!value.is<sol::table>()) {
            throw std::invalid_argument("expected a Lua map table");
        }
        const sol::table table = value.as<sol::table>();
        Map result;
        for (const auto& entry : table) {
            result.emplace(
                LuaCodecAdapter<Key, KeyPolicy>::read(entry.first, label),
                LuaCodecAdapter<Item, ItemPolicy>::read(entry.second, label));
        }
        return result;
    }

    static sol::object write(sol::state_view lua, const Map& value,
                             std::string_view label) {
        sol::table table = lua.create_table(0, static_cast<int>(value.size()));
        for (const auto& entry : value) {
            const sol::object key =
                LuaCodecAdapter<Key, KeyPolicy>::write(lua, entry.first, label);
            const sol::object item = LuaCodecAdapter<Item, ItemPolicy>::write(
                lua, entry.second, label);
            if (!isNil(item)) {
                table.raw_set(key, item);
            }
        }
        return sol::make_object(lua, table);
    }
};

template <typename Optional, typename ItemPolicy>
struct LuaCodecAdapter<Optional, LuaOptionalCodecPolicy<ItemPolicy>> {
    using Item = typename Optional::value_type;

    static_assert(IsOptional<Optional>::value);

    static bool canRead(const sol::object& value) {
        return isNil(value) ||
               LuaCodecAdapter<Item, ItemPolicy>::canRead(value);
    }

    static Optional read(const sol::object& value, std::string_view label) {
        if (isNil(value)) {
            return std::nullopt;
        }
        return Optional(LuaCodecAdapter<Item, ItemPolicy>::read(value, label));
    }

    static sol::object write(sol::state_view lua, const Optional& value,
                             std::string_view label) {
        if (!value.has_value()) {
            return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
        }
        return LuaCodecAdapter<Item, ItemPolicy>::write(lua, *value, label);
    }
};

template <typename Variant, typename... Policies>
struct LuaCodecAdapter<Variant, LuaVariantCodecPolicy<Policies...>> {
    using PolicyTuple = std::tuple<Policies...>;

    static_assert(IsVariant<Variant>::value);
    static_assert(std::variant_size_v<Variant> == sizeof...(Policies));

    static bool canRead(const sol::object& value) {
        return canReadAt<sizeof...(Policies)>(value);
    }

    static Variant read(const sol::object& value, std::string_view label) {
        return readAt<sizeof...(Policies)>(value, label);
    }

    static sol::object write(sol::state_view lua, const Variant& value,
                             std::string_view label) {
        if (value.valueless_by_exception()) {
            throw std::invalid_argument("cannot write a valueless variant");
        }
        return writeAt<0>(lua, value, label);
    }

private:
    template <std::size_t Index>
    static bool canReadAt(const sol::object& value) {
        if constexpr (Index == 0) {
            return false;
        } else {
            using Alternative = std::variant_alternative_t<Index - 1, Variant>;
            using Policy = std::tuple_element_t<Index - 1, PolicyTuple>;
            return LuaCodecAdapter<Alternative, Policy>::canRead(value) ||
                   canReadAt<Index - 1>(value);
        }
    }

    template <std::size_t Index>
    static Variant readAt(const sol::object& value, std::string_view label) {
        if constexpr (Index == 0) {
            throw std::invalid_argument(
                "Lua value does not match any variant alternative");
        } else {
            using Alternative = std::variant_alternative_t<Index - 1, Variant>;
            using Policy = std::tuple_element_t<Index - 1, PolicyTuple>;
            if (LuaCodecAdapter<Alternative, Policy>::canRead(value)) {
                return Variant(
                    std::in_place_index<Index - 1>,
                    LuaCodecAdapter<Alternative, Policy>::read(value, label));
            }
            return readAt<Index - 1>(value, label);
        }
    }

    template <std::size_t Index>
    static sol::object writeAt(sol::state_view lua, const Variant& value,
                               std::string_view label) {
        if constexpr (Index == sizeof...(Policies)) {
            throw std::invalid_argument("variant index is out of range");
        } else {
            if (value.index() == Index) {
                using Alternative = std::variant_alternative_t<Index, Variant>;
                using Policy = std::tuple_element_t<Index, PolicyTuple>;
                return LuaCodecAdapter<Alternative, Policy>::write(
                    lua, std::get<Index>(value), label);
            }
            return writeAt<Index + 1>(lua, value, label);
        }
    }
};

template <typename Pair, typename FirstPolicy, typename SecondPolicy>
struct LuaCodecAdapter<Pair, LuaPairCodecPolicy<FirstPolicy, SecondPolicy>> {
    using First = typename Pair::first_type;
    using Second = typename Pair::second_type;

    static_assert(IsPair<Pair>::value);

    static bool canRead(const sol::object& value) {
        if (!value.is<sol::table>()) {
            return false;
        }
        const sol::table table = value.as<sol::table>();
        std::size_t length = 0;
        return trySequenceLength(table, length) && length == 2 &&
               LuaCodecAdapter<First, FirstPolicy>::canRead(
                   table.raw_get<sol::object>(1)) &&
               LuaCodecAdapter<Second, SecondPolicy>::canRead(
                   table.raw_get<sol::object>(2));
    }

    static Pair read(const sol::object& value, std::string_view label) {
        if (!canRead(value)) {
            throw std::invalid_argument(
                "expected a compatible two-element Lua table");
        }
        const sol::table table = value.as<sol::table>();
        return Pair{LuaCodecAdapter<First, FirstPolicy>::read(
                        table.raw_get<sol::object>(1), label),
                    LuaCodecAdapter<Second, SecondPolicy>::read(
                        table.raw_get<sol::object>(2), label)};
    }

    static sol::object write(sol::state_view lua, const Pair& value,
                             std::string_view label) {
        sol::table table = lua.create_table(2, 1);
        table.raw_set("n", 2);
        const sol::object first =
            LuaCodecAdapter<First, FirstPolicy>::write(lua, value.first, label);
        const sol::object second = LuaCodecAdapter<Second, SecondPolicy>::write(
            lua, value.second, label);
        if (!isNil(first)) {
            table.raw_set(1, first);
        }
        if (!isNil(second)) {
            table.raw_set(2, second);
        }
        return sol::make_object(lua, table);
    }
};

template <typename Tuple, typename... Policies>
struct LuaCodecAdapter<Tuple, LuaTupleCodecPolicy<Policies...>> {
    using PolicyTuple = std::tuple<Policies...>;

    static_assert(IsTuple<Tuple>::value);
    static_assert(std::tuple_size_v<Tuple> == sizeof...(Policies));

    static bool canRead(const sol::object& value) {
        if (!value.is<sol::table>()) {
            return false;
        }
        const sol::table table = value.as<sol::table>();
        std::size_t length = 0;
        return trySequenceLength(table, length) &&
               length == sizeof...(Policies) &&
               canReadItems(table,
                            std::make_index_sequence<sizeof...(Policies)>{});
    }

    static Tuple read(const sol::object& value, std::string_view label) {
        if (!canRead(value)) {
            throw std::invalid_argument(
                "expected a compatible Lua tuple table");
        }
        return readItems(value.as<sol::table>(), label,
                         std::make_index_sequence<sizeof...(Policies)>{});
    }

    static sol::object write(sol::state_view lua, const Tuple& value,
                             std::string_view label) {
        sol::table table = lua.create_table(sizeof...(Policies), 1);
        table.raw_set("n", sizeof...(Policies));
        writeItems(lua, table, value, label,
                   std::make_index_sequence<sizeof...(Policies)>{});
        return sol::make_object(lua, table);
    }

private:
    template <std::size_t... Index>
    static bool canReadItems(const sol::table& table,
                             std::index_sequence<Index...>) {
        return (LuaCodecAdapter<std::tuple_element_t<Index, Tuple>,
                                std::tuple_element_t<Index, PolicyTuple>>::
                    canRead(table.raw_get<sol::object>(Index + 1)) &&
                ...);
    }

    template <std::size_t... Index>
    static Tuple readItems(const sol::table& table, std::string_view label,
                           std::index_sequence<Index...>) {
        return Tuple{LuaCodecAdapter<std::tuple_element_t<Index, Tuple>,
                                     std::tuple_element_t<Index, PolicyTuple>>::
                         read(table.raw_get<sol::object>(Index + 1), label)...};
    }

    template <std::size_t... Index>
    static void writeItems(sol::state_view lua, sol::table& table,
                           const Tuple& value, std::string_view label,
                           std::index_sequence<Index...>) {
        (
            [&]() {
                const sol::object item =
                    LuaCodecAdapter<std::tuple_element_t<Index, Tuple>,
                                    std::tuple_element_t<Index, PolicyTuple>>::
                        write(lua, std::get<Index>(value), label);
                if (!isNil(item)) {
                    table.raw_set(Index + 1, item);
                }
            }(),
            ...);
    }
};

template <typename T, typename Policy>
bool canReadLuaCodecValue(const sol::object& value) {
    return LuaCodecAdapter<LuaValueType<T>, Policy>::canRead(value);
}

template <typename T, typename Policy>
LuaValueType<T> readLuaCodecValue(const sol::object& value,
                                  std::string_view label) {
    return LuaCodecAdapter<LuaValueType<T>, Policy>::read(value, label);
}

template <typename T, typename Policy>
sol::object writeLuaCodecValue(sol::state_view lua, const T& value,
                               std::string_view label) {
    return LuaCodecAdapter<LuaValueType<T>, Policy>::write(lua, value, label);
}

template <typename Policy>
struct LuaCodecReturnPolicies;

template <typename First, typename Second>
struct LuaCodecReturnPolicies<LuaPairCodecPolicy<First, Second>> {
    using Type = std::tuple<First, Second>;
};

template <typename... Items>
struct LuaCodecReturnPolicies<LuaTupleCodecPolicy<Items...>> {
    using Type = std::tuple<Items...>;
};

template <typename Values, typename Policy, std::size_t... Index>
LuaReturnTuple<Values> writeLuaCodecReturnsImpl(sol::state_view lua,
                                                const Values& values,
                                                std::string_view label,
                                                std::index_sequence<Index...>) {
    using Value = LuaValueType<Values>;
    using Policies = typename LuaCodecReturnPolicies<Policy>::Type;
    return LuaReturnTuple<Value>{LuaCodecAdapter<
        std::tuple_element_t<Index, Value>,
        std::tuple_element_t<Index, Policies>>::write(lua,
                                                      std::get<Index>(values),
                                                      label)...};
}

template <typename Values, typename Policy>
LuaReturnTuple<Values> writeLuaCodecReturns(sol::state_view lua,
                                            const Values& values,
                                            std::string_view label) {
    using Value = LuaValueType<Values>;
    using Policies = typename LuaCodecReturnPolicies<Policy>::Type;
    static_assert(std::tuple_size_v<Value> == std::tuple_size_v<Policies>);
    return writeLuaCodecReturnsImpl<Value, Policy>(
        lua, values, label,
        std::make_index_sequence<std::tuple_size_v<Value>>{});
}

}  // namespace ludork::runtime::binding
