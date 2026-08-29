#pragma once

#include <utils.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace ludork_core {

template <typename T, typename = void>
struct DynamicValueTraits {
    static constexpr bool enabled = false;
};

template <typename T>
struct DynamicValueTraits<T,
                          std::void_t<typename T::LuaBindingDynamicValueTag>> {
    static constexpr bool enabled = true;
};

template <typename T>
struct TableValueTraits {
    static constexpr bool enabled = false;
};

template <typename T, typename = void>
struct OpaqueIdentityTraits {
    static constexpr bool enabled = false;
};

template <typename T>
struct OpaqueIdentityTraits<
    T, std::void_t<typename T::LuaBindingOpaqueIdentityTag>> {
    static constexpr bool enabled = true;
};

template <typename T>
struct StdFunctionTraits {
    static constexpr bool enabled = false;
};

template <typename Signature>
struct StdFunctionTraits<std::function<Signature>> {
    static constexpr bool enabled = true;
    using FunctionSignature = Signature;
};

template <typename T>
using LuaValueType = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename T>
struct IsVector : std::false_type {};

template <typename T, typename Allocator>
struct IsVector<std::vector<T, Allocator>> : std::true_type {};

template <typename T>
struct IsArray : std::false_type {};

template <typename T, std::size_t Size>
struct IsArray<std::array<T, Size>> : std::true_type {};

template <typename T>
struct IsPair : std::false_type {};

template <typename First, typename Second>
struct IsPair<std::pair<First, Second>> : std::true_type {};

template <typename T>
struct IsTuple : std::false_type {};

template <typename... Values>
struct IsTuple<std::tuple<Values...>> : std::true_type {};

template <typename T>
struct LuaReturnTupleType;

template <typename... Values>
struct LuaReturnTupleType<std::tuple<Values...>> {
    template <typename>
    using Object = sol::object;
    using Type = std::tuple<Object<Values>...>;
};

template <typename First, typename Second>
struct LuaReturnTupleType<std::pair<First, Second>> {
    using Type = std::tuple<sol::object, sol::object>;
};

template <typename T>
using LuaReturnTuple = typename LuaReturnTupleType<LuaValueType<T>>::Type;

template <typename T>
struct IsMap : std::false_type {};

template <typename Key, typename Value, typename Compare, typename Allocator>
struct IsMap<std::map<Key, Value, Compare, Allocator>> : std::true_type {};

template <typename Key, typename Value, typename Hash, typename Equal,
          typename Allocator>
struct IsMap<std::unordered_map<Key, Value, Hash, Equal, Allocator>>
    : std::true_type {};

template <typename T>
struct IsOptional : std::false_type {};

template <typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template <typename T>
struct IsSharedPointer : std::false_type {};

template <typename T>
struct IsSharedPointer<std::shared_ptr<T>> : std::true_type {
    using Element = T;
};

template <typename T>
struct IsOpaqueIdentityPointer : std::false_type {};

template <typename T>
struct IsOpaqueIdentityPointer<std::shared_ptr<T>>
    : std::bool_constant<OpaqueIdentityTraits<T>::enabled> {};

template <typename... Values>
struct FirstOpaqueIdentity {
    using Type = void;
};

template <typename Value, typename... Rest>
struct FirstOpaqueIdentity<Value, Rest...> {
    using Type =
        std::conditional_t<IsOpaqueIdentityPointer<Value>::value, Value,
                           typename FirstOpaqueIdentity<Rest...>::Type>;
};

template <typename Storage>
struct DynamicIdentityType;

template <typename... Values>
struct DynamicIdentityType<std::variant<Values...>> {
    using Type = typename FirstOpaqueIdentity<Values...>::Type;
};

template <typename T>
struct IsVariant : std::false_type {};

template <typename... T>
struct IsVariant<std::variant<T...>> : std::true_type {};

template <typename T>
inline constexpr bool IsOptionalValue = IsOptional<LuaValueType<T>>::value;

template <typename T>
inline constexpr bool IsDynamicValue =
    DynamicValueTraits<LuaValueType<T>>::enabled;

template <typename T>
inline constexpr bool IsTableValue = TableValueTraits<LuaValueType<T>>::enabled;

template <typename T>
inline constexpr bool IsStdFunction =
    StdFunctionTraits<LuaValueType<T>>::enabled;

template <typename DynamicValue>
bool canReadDynamicValue(const sol::object& value);

template <typename DynamicValue>
DynamicValue readDynamicValue(const sol::object& value);

template <typename DynamicValue>
sol::object writeDynamicValue(sol::state_view lua, const DynamicValue& value);

template <typename Native>
bool tryReadNativeValue(const sol::object& value, Native& result);

template <typename Pointer>
Pointer readOpaqueIdentity(const sol::object& value);

template <typename Pointer>
sol::object writeOpaqueIdentity(sol::state_view lua, const Pointer& value);

template <typename Pointer>
Pointer readSharedPointer(const sol::object& value);

template <typename Pointer>
bool tryReadPointer(const sol::object& value, Pointer& result);

template <typename Pointer>
Pointer readPointer(const sol::object& value);

sol::object nativePointerOwner(sol::state_view lua, const void* pointer);

int pushNativePointerOwnerTable(sol::state_view lua);

bool pushNativePointerOwnerFromTable(lua_State* state, int tableIndex,
                                     const void* pointer);

template <typename T, typename... Bases>
sol::object writeOwningLuaObject(sol::state_view lua,
                                 const std::shared_ptr<T>& value);

bool tryWriteDynamicNativeObject(sol::state_view lua,
                                 const std::type_info& dynamicType,
                                 const std::shared_ptr<void>& owner,
                                 sol::object& result);

template <typename Signature>
std::function<Signature> functionFromLua(const sol::object& value);

template <typename Signature>
sol::object functionToLua(sol::state_view lua,
                          const std::function<Signature>& value);

inline bool isNil(const sol::object& value) {
    return !value.valid() || value.get_type() == sol::type::none ||
           value.get_type() == sol::type::lua_nil;
}

inline bool trySequenceLength(const sol::table& value, std::size_t& length) {
    const sol::object rawLength = value.raw_get<sol::object>("n");
    if (isNil(rawLength)) {
        length = value.size();
        return true;
    }
    if (!rawLength.is<lua_sf::LuaIntegral<std::size_t>>()) {
        return false;
    }
    length = rawLength.as<lua_sf::LuaIntegral<std::size_t>>().value();
    return true;
}

template <typename T>
bool canReadLuaValue(const sol::object& value);

template <typename T>
T readLuaValue(const sol::object& value);

template <typename T>
sol::object writeLuaValue(sol::state_view lua, const T& value);

template <typename Return, typename... Arguments>
Return callPushedLuaFunction(lua_State* state, Arguments&&... arguments);

template <typename T, bool AllowNil = false>
class LuaArgument {
public:
    explicit LuaArgument(sol::object value) : value_(std::move(value)) {}

    [[nodiscard]] T value() const {
        return readLuaValue<T>(value_);
    }
    [[nodiscard]] const sol::object& object() const {
        return value_;
    }

private:
    sol::object value_;
};

template <typename T, bool AllowNil, typename Handler>
bool sol_lua_check(sol::types<LuaArgument<T, AllowNil>>, lua_State* state,
                   int index, Handler&& handler, sol::stack::record& tracking) {
    tracking.use(1);
    bool success = false;
    {
        const sol::object value = sol::stack::get<sol::object>(state, index);
        success = (AllowNil && isNil(value)) || canReadLuaValue<T>(value);
    }
    if (!success) {
        handler(state, index, sol::type::poly, sol::type_of(state, index),
                "value is not compatible with the requested C++ type");
    }
    return success;
}

template <typename T, bool AllowNil>
LuaArgument<T, AllowNil> sol_lua_get(sol::types<LuaArgument<T, AllowNil>>,
                                     lua_State* state, int index,
                                     sol::stack::record& tracking) {
    tracking.use(1);
    sol::object value = sol::stack::get<sol::object>(state, index);
    if (!((AllowNil && isNil(value)) || canReadLuaValue<T>(value))) {
        throw std::invalid_argument(
            "value is not compatible with the requested C++ type");
    }
    return LuaArgument<T, AllowNil>(std::move(value));
}

inline bool luaIntegerValue(const sol::object& value, std::int64_t& result) {
    lua_State* state = value.lua_state();
    value.push();
    const bool isInteger = lua_isinteger(state, -1) != 0;
    if (isInteger) {
        result = static_cast<std::int64_t>(lua_tointeger(state, -1));
    }
    lua_pop(state, 1);
    return isInteger;
}

template <typename Values>
LuaReturnTuple<Values> writeLuaReturns(sol::state_view lua,
                                       const Values& values) {
    return std::apply(
        [lua](const auto&... items) {
            return LuaReturnTuple<Values>{writeLuaValue(lua, items)...};
        },
        values);
}

template <typename Sequence>
bool canReadSequence(const sol::object& value,
                     std::optional<std::size_t> fixedLength = std::nullopt) {
    if (!value.is<sol::table>()) {
        return false;
    }
    const sol::table table = value.as<sol::table>();
    std::size_t length = 0;
    if (!trySequenceLength(table, length)) {
        return false;
    }
    if (fixedLength.has_value() && length != *fixedLength) {
        return false;
    }
    using Item = typename Sequence::value_type;
    for (std::size_t index = 1; index <= length; ++index) {
        const sol::object item = table.raw_get<sol::object>(index);
        if (isNil(item)) {
            if constexpr (!IsOptionalValue<Item> && !IsDynamicValue<Item>) {
                return false;
            }
            continue;
        }
        if (!canReadLuaValue<Item>(item)) {
            return false;
        }
    }
    return true;
}

template <typename Map>
bool canReadMap(const sol::object& value) {
    if (!value.is<sol::table>()) {
        return false;
    }
    const sol::table table = value.as<sol::table>();
    using Key = typename Map::key_type;
    using Value = typename Map::mapped_type;
    for (const auto& entry : table) {
        const sol::object key = entry.first;
        const sol::object item = entry.second;
        if (!canReadLuaValue<Key>(key)) {
            return false;
        }
        if (isNil(item)) {
            if constexpr (!IsOptionalValue<Value>) {
                return false;
            }
            continue;
        }
        if (!canReadLuaValue<Value>(item)) {
            return false;
        }
    }
    return true;
}

template <typename Variant, std::size_t Index>
bool canReadVariant(const sol::object& value) {
    if constexpr (Index == 0) {
        return false;
    } else {
        using Alternative = std::variant_alternative_t<Index - 1, Variant>;
        if (canReadLuaValue<Alternative>(value)) {
            return true;
        }
        return canReadVariant<Variant, Index - 1>(value);
    }
}

template <typename Tuple, std::size_t... Index>
bool canReadTuple(const sol::table& table, std::index_sequence<Index...>) {
    return (canReadLuaValue<std::tuple_element_t<Index, Tuple>>(
                table.raw_get<sol::object>(Index + 1)) &&
            ...);
}

template <typename T>
bool canReadLuaValue(const sol::object& value) {
    using Value = LuaValueType<T>;
    if constexpr (IsDynamicValue<Value>) {
        return canReadDynamicValue<Value>(value);
    } else if constexpr (IsTableValue<Value>) {
        return TableValueTraits<Value>::canRead(value);
    } else if constexpr (IsOpaqueIdentityPointer<Value>::value) {
        return value.valid();
    } else if constexpr (IsSharedPointer<Value>::value) {
        if (isNil(value)) {
            return true;
        }
        Value pointer;
        return tryReadNativeValue(value, pointer);
    } else if constexpr (std::is_pointer_v<Value>) {
        Value pointer = nullptr;
        return tryReadPointer(value, pointer);
    } else if constexpr (IsStdFunction<Value>) {
        return isNil(value) || value.is<sol::protected_function>();
    } else if constexpr (IsVector<Value>::value) {
        return canReadSequence<Value>(value);
    } else if constexpr (IsArray<Value>::value) {
        return canReadSequence<Value>(value, std::tuple_size_v<Value>);
    } else if constexpr (IsPair<Value>::value) {
        if (!value.is<sol::table>()) {
            return false;
        }
        const sol::table table = value.as<sol::table>();
        std::size_t length = 0;
        if (!trySequenceLength(table, length) || length != 2) {
            return false;
        }
        return canReadLuaValue<typename Value::first_type>(
                   table.raw_get<sol::object>(1)) &&
               canReadLuaValue<typename Value::second_type>(
                   table.raw_get<sol::object>(2));
    } else if constexpr (IsTuple<Value>::value) {
        if (!value.is<sol::table>()) {
            return false;
        }
        const sol::table table = value.as<sol::table>();
        std::size_t length = 0;
        if (!trySequenceLength(table, length) ||
            length != std::tuple_size_v<Value>) {
            return false;
        }
        return canReadTuple<Value>(
            table, std::make_index_sequence<std::tuple_size_v<Value>>{});
    } else if constexpr (IsMap<Value>::value) {
        return canReadMap<Value>(value);
    } else if constexpr (IsOptional<Value>::value) {
        return isNil(value) ||
               canReadLuaValue<typename Value::value_type>(value);
    } else if constexpr (IsVariant<Value>::value) {
        return canReadVariant<Value, std::variant_size_v<Value>>(value);
    } else if constexpr (std::is_same_v<Value, sol::object>) {
        return value.valid();
    } else if constexpr (std::is_same_v<Value, sol::table>) {
        return value.is<sol::table>();
    } else if constexpr (lua_sf::is_lua_integral_v<Value>) {
        return value.is<lua_sf::LuaIntegral<Value>>();
    } else {
        return value.is<Value>();
    }
}

template <typename Sequence>
Sequence readSequence(const sol::object& value,
                      std::optional<std::size_t> fixedLength = std::nullopt) {
    if (!value.is<sol::table>()) {
        throw std::invalid_argument("expected a Lua sequence table");
    }
    const sol::table table = value.as<sol::table>();
    std::size_t length = 0;
    if (!trySequenceLength(table, length)) {
        throw std::invalid_argument(
            "Lua sequence n must be a non-negative integer");
    }
    if (fixedLength.has_value() && length != *fixedLength) {
        throw std::invalid_argument(
            "Lua sequence length does not match the fixed C++ size");
    }
    Sequence result{};
    if constexpr (IsVector<Sequence>::value) {
        result.reserve(length);
    }
    using Item = typename Sequence::value_type;
    for (std::size_t index = 1; index <= length; ++index) {
        const sol::object item = table.raw_get<sol::object>(index);
        if (isNil(item) && !IsOptionalValue<Item> && !IsDynamicValue<Item>) {
            throw std::invalid_argument("Lua sequence contains nil at index " +
                                        std::to_string(index));
        }
        try {
            if constexpr (IsVector<Sequence>::value) {
                result.push_back(readLuaValue<Item>(item));
            } else {
                result[index - 1] = readLuaValue<Item>(item);
            }
        } catch (const std::exception& error) {
            throw std::invalid_argument("Lua sequence element at index " +
                                        std::to_string(index) + ": " +
                                        error.what());
        }
    }
    return result;
}

template <typename Map>
Map readMap(const sol::object& value) {
    if (!value.is<sol::table>()) {
        throw std::invalid_argument("expected a Lua map table");
    }
    const sol::table table = value.as<sol::table>();
    Map result;
    using Key = typename Map::key_type;
    using Item = typename Map::mapped_type;
    for (const auto& entry : table) {
        const sol::object key = entry.first;
        const sol::object item = entry.second;
        if (isNil(item) && !IsOptionalValue<Item>) {
            throw std::invalid_argument("Lua map contains a nil value");
        }
        result.emplace(readLuaValue<Key>(key), readLuaValue<Item>(item));
    }
    return result;
}

template <typename Variant, std::size_t Index>
Variant readVariant(const sol::object& value) {
    if constexpr (Index == 0) {
        throw std::invalid_argument(
            "Lua value does not match any variant alternative");
    } else {
        using Alternative = std::variant_alternative_t<Index - 1, Variant>;
        if (canReadLuaValue<Alternative>(value)) {
            return Variant(std::in_place_index<Index - 1>,
                           readLuaValue<Alternative>(value));
        }
        return readVariant<Variant, Index - 1>(value);
    }
}

template <typename Tuple, std::size_t... Index>
Tuple readTuple(const sol::table& table, std::index_sequence<Index...>) {
    return Tuple{readLuaValue<std::tuple_element_t<Index, Tuple>>(
        table.raw_get<sol::object>(Index + 1))...};
}

template <typename T>
T readLuaValue(const sol::object& value) {
    using Value = LuaValueType<T>;
    if constexpr (IsDynamicValue<Value>) {
        return readDynamicValue<Value>(value);
    } else if constexpr (IsTableValue<Value>) {
        return TableValueTraits<Value>::read(value);
    } else if constexpr (IsOpaqueIdentityPointer<Value>::value) {
        return readOpaqueIdentity<Value>(value);
    } else if constexpr (IsSharedPointer<Value>::value) {
        return readSharedPointer<Value>(value);
    } else if constexpr (std::is_pointer_v<Value>) {
        return readPointer<Value>(value);
    } else if constexpr (IsStdFunction<Value>) {
        using Signature = typename StdFunctionTraits<Value>::FunctionSignature;
        return functionFromLua<Signature>(value);
    } else if constexpr (IsVector<Value>::value) {
        return readSequence<Value>(value);
    } else if constexpr (IsArray<Value>::value) {
        return readSequence<Value>(value, std::tuple_size_v<Value>);
    } else if constexpr (IsPair<Value>::value) {
        if (!value.is<sol::table>()) {
            throw std::invalid_argument("expected a two-element Lua table");
        }
        const sol::table table = value.as<sol::table>();
        std::size_t length = 0;
        if (!trySequenceLength(table, length) || length != 2) {
            throw std::invalid_argument(
                "Lua pair table must contain exactly two values");
        }
        auto first = [&table]() {
            try {
                return readLuaValue<typename Value::first_type>(
                    table.raw_get<sol::object>(1));
            } catch (const std::exception& error) {
                throw std::invalid_argument("Lua pair element at index 1: " +
                                            std::string(error.what()));
            }
        }();
        auto second = [&table]() {
            try {
                return readLuaValue<typename Value::second_type>(
                    table.raw_get<sol::object>(2));
            } catch (const std::exception& error) {
                throw std::invalid_argument("Lua pair element at index 2: " +
                                            std::string(error.what()));
            }
        }();
        return Value(std::move(first), std::move(second));
    } else if constexpr (IsTuple<Value>::value) {
        if (!value.is<sol::table>()) {
            throw std::invalid_argument("expected a Lua tuple table");
        }
        const sol::table table = value.as<sol::table>();
        std::size_t length = 0;
        if (!trySequenceLength(table, length) ||
            length != std::tuple_size_v<Value>) {
            throw std::invalid_argument(
                "Lua tuple length does not match the fixed C++ size");
        }
        return readTuple<Value>(
            table, std::make_index_sequence<std::tuple_size_v<Value>>{});
    } else if constexpr (IsMap<Value>::value) {
        return readMap<Value>(value);
    } else if constexpr (IsOptional<Value>::value) {
        if (isNil(value)) {
            return std::nullopt;
        }
        return Value(readLuaValue<typename Value::value_type>(value));
    } else if constexpr (IsVariant<Value>::value) {
        return readVariant<Value, std::variant_size_v<Value>>(value);
    } else if constexpr (std::is_same_v<Value, sol::object>) {
        return value;
    } else {
        if (!canReadLuaValue<Value>(value)) {
            throw std::invalid_argument(
                "Lua value is not compatible with the requested native type");
        }
        return lua_sf::object_as<Value>(value);
    }
}

template <typename Sequence>
sol::object writeSequence(sol::state_view lua, const Sequence& value) {
    lua_State* state = lua.lua_state();
    const int stackTop = lua_gettop(state);
    struct StackRestore {
        lua_State* state;
        int top;

        ~StackRestore() noexcept {
            lua_settop(state, top);
        }
    } stackRestore{state, stackTop};
    using Item = typename Sequence::value_type;
    constexpr bool NativeOwnerItem =
        (IsSharedPointer<Item>::value &&
         !IsOpaqueIdentityPointer<Item>::value) ||
        (std::is_pointer_v<Item> &&
         std::is_object_v<std::remove_pointer_t<Item>>);
    constexpr bool StoresLength = IsOptionalValue<Item> || IsDynamicValue<Item>;
    lua_createtable(state, static_cast<int>(value.size()),
                    StoresLength ? 1 : 0);
    const int tableIndex = lua_absindex(state, -1);
    if constexpr (IsOptionalValue<Item> || IsDynamicValue<Item>) {
        lua_pushliteral(state, "n");
        lua_pushinteger(state, static_cast<lua_Integer>(value.size()));
        lua_rawset(state, tableIndex);
    }
    int ownerTableIndex = 0;
    if constexpr (NativeOwnerItem) {
        ownerTableIndex = pushNativePointerOwnerTable(lua);
    }
    std::size_t index = 1;
    for (const auto& item : value) {
        bool pushed = false;
        if constexpr (NativeOwnerItem) {
            const void* pointer = nullptr;
            if constexpr (IsSharedPointer<Item>::value) {
                pointer = item.get();
            } else {
                pointer = item;
            }
            pushed = pushNativePointerOwnerFromTable(state, ownerTableIndex,
                                                     pointer);
        }
        sol::object output;
        if constexpr (std::is_same_v<Item, bool>) {
            const bool converted = static_cast<bool>(item);
            output = writeLuaValue(lua, converted);
        } else if (!pushed) {
            output = writeLuaValue(lua, item);
        }
        if (!pushed && !isNil(output)) {
            output.push(state);
            pushed = true;
        }
        if (pushed) {
            lua_rawseti(state, tableIndex, static_cast<lua_Integer>(index));
        }
        ++index;
    }
    sol::object result = sol::stack::get<sol::object>(state, tableIndex);
    return result;
}

template <typename Map>
sol::object writeMap(sol::state_view lua, const Map& value) {
    sol::table table = lua.create_table(0, static_cast<int>(value.size()));
    for (const auto& entry : value) {
        const sol::object key = writeLuaValue(lua, entry.first);
        const sol::object item = writeLuaValue(lua, entry.second);
        if (!isNil(item)) {
            table.raw_set(key, item);
        }
    }
    return sol::make_object(lua, table);
}

template <typename Tuple, std::size_t... Index>
sol::object writeTuple(sol::state_view lua, const Tuple& value,
                       std::index_sequence<Index...>) {
    sol::table table = lua.create_table(static_cast<int>(sizeof...(Index)), 1);
    table.raw_set("n", sizeof...(Index));
    ((table.raw_set(Index + 1, writeLuaValue(lua, std::get<Index>(value)))),
     ...);
    return sol::make_object(lua, table);
}

template <typename T>
sol::object writeLuaValue(sol::state_view lua, const T& value) {
    using Value = LuaValueType<T>;
    if constexpr (IsDynamicValue<Value>) {
        return writeDynamicValue(lua, value);
    } else if constexpr (IsTableValue<Value>) {
        return TableValueTraits<Value>::write(lua, value);
    } else if constexpr (IsOpaqueIdentityPointer<Value>::value) {
        return writeOpaqueIdentity(lua, value);
    } else if constexpr (IsStdFunction<Value>) {
        using Signature = typename StdFunctionTraits<Value>::FunctionSignature;
        return functionToLua<Signature>(lua, value);
    } else if constexpr (IsVector<Value>::value || IsArray<Value>::value) {
        return writeSequence(lua, value);
    } else if constexpr (IsPair<Value>::value) {
        sol::table table = lua.create_table(2, 0);
        table.raw_set("n", 2);
        table.raw_set(1, writeLuaValue(lua, value.first));
        table.raw_set(2, writeLuaValue(lua, value.second));
        return sol::make_object(lua, table);
    } else if constexpr (IsTuple<Value>::value) {
        return writeTuple(lua, value,
                          std::make_index_sequence<std::tuple_size_v<Value>>{});
    } else if constexpr (IsMap<Value>::value) {
        return writeMap(lua, value);
    } else if constexpr (IsOptional<Value>::value) {
        if (!value.has_value()) {
            return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
        }
        return writeLuaValue(lua, *value);
    } else if constexpr (IsVariant<Value>::value) {
        return std::visit(
            [lua](const auto& item) {
                return writeLuaValue(lua, item);
            },
            value);
    } else if constexpr (std::is_same_v<Value, sol::object>) {
        return value;
    } else if constexpr (IsSharedPointer<Value>::value) {
        if (!value) {
            return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
        }
        const sol::object owner = nativePointerOwner(lua, value.get());
        if (!isNil(owner)) {
            return owner;
        }
        using Element = typename IsSharedPointer<Value>::Element;
        if constexpr (std::is_polymorphic_v<Element> &&
                      !std::is_const_v<Element>) {
            void* completeObject = dynamic_cast<void*>(value.get());
            const std::shared_ptr<void> dynamicOwner(value, completeObject);
            sol::object dynamicValue;
            if (tryWriteDynamicNativeObject(lua, typeid(*value), dynamicOwner,
                                            dynamicValue)) {
                return dynamicValue;
            }
        }
        return writeOwningLuaObject<Element>(lua, value);
    } else if constexpr (std::is_pointer_v<Value>) {
        if (value == nullptr) {
            return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
        }
        const sol::object owner = nativePointerOwner(lua, value);
        if (!isNil(owner)) {
            return owner;
        }
        return lua_sf::as_lua_object(lua, value);
    } else {
        return lua_sf::as_lua_object(lua, value);
    }
}

}  // namespace ludork_core
