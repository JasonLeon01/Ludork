#pragma once

#include <ClassServices.hpp>
#include <LuaError.hpp>
#include <RuntimeSession.hpp>
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
#include <unordered_set>
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

inline ludork::standard::LuaRegistryReference makeLuaRegistryReference(
    const sol::object& value) {
    lua_State* state = value.lua_state();
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active()) {
        throw std::runtime_error("Lua runtime session is stopping");
    }
    auto pushed = sol::stack::push_pop(value);
    return ludork::standard::LuaRegistryReference(state,
                                                  pushed.index_of(value));
}

inline ludork::standard::LuaRegistryReference makeLuaCallbackReference(
    const sol::table& callbacks, const char* name) {
    const sol::object callback = callbacks.raw_get<sol::object>(name);
    if (!callback.is<sol::protected_function>()) {
        return {};
    }
    return makeLuaRegistryReference(callback);
}

inline sol::object readLuaRegistryReference(
    sol::state_view lua,
    const ludork::standard::LuaRegistryReference& reference) {
    lua_State* state = lua.lua_state();
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active()) {
        throw std::runtime_error("Lua runtime session is stopping");
    }
    if (reference.state() != state) {
        throw std::invalid_argument(
            "Lua registry reference belongs to another state");
    }
    if (!reference.push()) {
        throw std::runtime_error(
            "Lua registry reference is no longer available");
    }
    auto popper = sol::stack::pop_n(state, 1);
    return sol::stack::get<sol::object>(state, -1);
}

template <typename Base>
class LuaOpaqueObject final : public Base {
public:
    explicit LuaOpaqueObject(const sol::object& value)
        : value_(makeLuaRegistryReference(value)) {
        ludork::standard::registerRuntimeOpaqueValue(this, value_);
    }

    ~LuaOpaqueObject() {
        ludork::standard::unregisterRuntimeOpaqueValue(value_.state(), this);
    }

    [[nodiscard]] const ludork::standard::LuaRegistryReference& value() const {
        return value_;
    }

private:
    ludork::standard::LuaRegistryReference value_;
};

template <typename Base>
class LuaOpaqueIdentity final : public Base {
public:
    explicit LuaOpaqueIdentity(const sol::object& value)
        : value_(makeLuaRegistryReference(value)) {
        ludork::standard::registerRuntimeOpaqueValue(this, value_);
    }

    ~LuaOpaqueIdentity() {
        ludork::standard::unregisterRuntimeOpaqueValue(value_.state(), this);
    }

    bool equals(const Base& other) const override {
        const ludork::standard::LuaRegistryReference reference =
            ludork::standard::findRuntimeOpaqueValue(value_.state(), &other);
        return reference && value_.equals(reference);
    }

    [[nodiscard]] const ludork::standard::LuaRegistryReference& value() const {
        return value_;
    }

private:
    ludork::standard::LuaRegistryReference value_;
};

template <typename Native>
bool tryReadNativeValue(const sol::object& value, Native& result);

template <typename T, typename... Bases>
sol::object writeOwningLuaObject(sol::state_view lua,
                                 const std::shared_ptr<T>& value);

template <typename Pointer>
Pointer readOpaqueIdentity(const sol::object& value) {
    using Base = typename IsSharedPointer<Pointer>::Element;
    if (!value.valid() || value.get_type() == sol::type::none ||
        value.get_type() == sol::type::lua_nil) {
        return Pointer{};
    }
    Pointer nativeValue;
    if (value.get_type() == sol::type::userdata &&
        tryReadNativeValue(value, nativeValue)) {
        return nativeValue;
    }
    return std::make_shared<LuaOpaqueIdentity<Base>>(value);
}

template <typename Pointer>
sol::object writeOpaqueIdentity(sol::state_view lua, const Pointer& value) {
    using Base = typename IsSharedPointer<Pointer>::Element;
    if (!value) {
        return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
    }
    const ludork::standard::LuaRegistryReference reference =
        ludork::standard::findRuntimeOpaqueValue(lua.lua_state(), value.get());
    if (reference) {
        return readLuaRegistryReference(lua, reference);
    }
    return writeOwningLuaObject<Base>(lua, value);
}

inline bool isNil(const sol::object& value) {
    return !value.valid() || value.get_type() == sol::type::none ||
           value.get_type() == sol::type::lua_nil;
}

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

inline sol::object nativePointerOwner(sol::state_view lua,
                                      const void* pointer) {
    if (pointer == nullptr) {
        return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
    }
    const sol::object rawOwners =
        lua.registry().raw_get<sol::object>("Ludork.Class.nativePointerOwners");
    if (!rawOwners.is<sol::table>()) {
        return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
    }
    lua_State* state = lua.lua_state();
    rawOwners.as<sol::table>().push();
    lua_pushlightuserdata(state, const_cast<void*>(pointer));
    lua_rawget(state, -2);
    sol::object result = sol::stack::get<sol::object>(state, -1);
    lua_pop(state, 2);
    return result;
}

inline void registerNativePointerOwner(sol::state_view lua, const void* pointer,
                                       const sol::object& owner) {
    if (pointer == nullptr || isNil(owner)) {
        return;
    }
    lua_State* state = lua.lua_state();
    lua_getfield(state, LUA_REGISTRYINDEX, "Ludork.Class.nativePointerOwners");
    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_newtable(state);
        lua_pushliteral(state, "__mode");
        lua_pushliteral(state, "v");
        lua_rawset(state, -3);
        lua_setmetatable(state, -2);
        lua_pushvalue(state, -1);
        lua_setfield(state, LUA_REGISTRYINDEX,
                     "Ludork.Class.nativePointerOwners");
    }
    lua_pushlightuserdata(state, const_cast<void*>(pointer));
    owner.push();
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

template <typename T, typename... Bases>
sol::object writeOwningLuaObject(sol::state_view lua,
                                 const std::shared_ptr<T>& value) {
    if (!value) {
        return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
    }
    if constexpr (requires { value->bindRuntimeOwner(value); }) {
        value->bindRuntimeOwner(value);
    }
    const sol::object owner =
        sol::make_object(lua, lua_sf::wrapLuaSharedObject(value));
    lua_sf::mark_shared_usertype<T>(lua);
    registerNativePointerOwner(lua, value.get(), owner);
    (registerNativePointerOwner(lua, static_cast<Bases*>(value.get()), owner),
     ...);
    return owner;
}

template <typename Dynamic, typename Exposed, typename... Bases>
int writeDynamicNativeObject(lua_State* state) {
    try {
        if (lua_type(state, 1) != LUA_TLIGHTUSERDATA) {
            return luaL_error(state,
                              "Dynamic native writer requires an owner token");
        }
        const auto* owner =
            static_cast<const std::shared_ptr<void>*>(lua_touserdata(state, 1));
        if (owner == nullptr || owner->get() == nullptr) {
            return luaL_error(state,
                              "Dynamic native writer owner is unavailable");
        }
        Dynamic* dynamic = static_cast<Dynamic*>(owner->get());
        Exposed* exposed = static_cast<Exposed*>(dynamic);
        const std::shared_ptr<Exposed> value(*owner, exposed);
        writeOwningLuaObject<Exposed, Bases...>(sol::state_view(state), value)
            .push();
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

template <typename Dynamic, typename Exposed, typename... Bases>
void registerDynamicNativeWriter(sol::state_view lua) {
    lua_State* state = lua.lua_state();
    lua_getfield(state, LUA_REGISTRYINDEX, "Ludork.Class.dynamicNativeWriters");
    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushvalue(state, -1);
        lua_setfield(state, LUA_REGISTRYINDEX,
                     "Ludork.Class.dynamicNativeWriters");
    }
    const int writersIndex = lua_absindex(state, -1);
    const char* dynamicName = typeid(Dynamic).name();
    lua_pushstring(state, dynamicName);
    lua_rawget(state, writersIndex);
    const lua_CFunction writer =
        &writeDynamicNativeObject<Dynamic, Exposed, Bases...>;
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        lua_pushstring(state, dynamicName);
        lua_pushcclosure(state, writer, 0);
        lua_rawset(state, writersIndex);
        lua_pop(state, 1);
        return;
    }
    const bool matches =
        lua_iscfunction(state, -1) != 0 && lua_tocfunction(state, -1) == writer;
    lua_pop(state, 2);
    if (!matches) {
        throw std::runtime_error(
            std::string("Dynamic native writer collision: ") + dynamicName);
    }
}

inline bool tryWriteDynamicNativeObject(sol::state_view lua,
                                        const std::type_info& dynamicType,
                                        const std::shared_ptr<void>& owner,
                                        sol::object& result) {
    lua_State* state = lua.lua_state();
    const int stackTop = lua_gettop(state);
    lua_getfield(state, LUA_REGISTRYINDEX, "Ludork.Class.dynamicNativeWriters");
    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_settop(state, stackTop);
        return false;
    }
    lua_pushstring(state, dynamicType.name());
    lua_rawget(state, -2);
    if (!lua_isfunction(state, -1)) {
        lua_settop(state, stackTop);
        return false;
    }
    lua_pushlightuserdata(
        state, const_cast<std::shared_ptr<void>*>(std::addressof(owner)));
    if (ludork::standard::protectedLuaCall(state, 1, 1) != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        const std::string error =
            message == nullptr ? "Dynamic native writer failed" : message;
        lua_settop(state, stackTop);
        throw std::runtime_error(error);
    }
    result = sol::stack::get<sol::object>(state, -1);
    lua_settop(state, stackTop);
    return true;
}

template <typename Native>
bool tryReadNativeValue(const sol::object& value, Native& result) {
    if constexpr (IsSharedPointer<Native>::value) {
        using Element = typename IsSharedPointer<Native>::Element;
        lua_State* state = value.lua_state();
        value.push();
        const lua_sf::detail::LuaSFNativeLookup lookup =
            lua_sf::detail::push_luasf_native_object<Element>(state, -1);
        if (lookup == lua_sf::detail::LuaSFNativeLookup::found ||
            lookup == lua_sf::detail::LuaSFNativeLookup::external) {
            const bool success =
                lua_sf::detail::get_pushed_luasf_shared_object<Element>(state,
                                                                        result);
            lua_pop(state, 2);
            if (success) {
                return true;
            }
        } else {
            lua_pop(state, 1);
        }
        return false;
    } else if constexpr (std::is_pointer_v<Native>) {
        using Element = std::remove_pointer_t<Native>;
        lua_State* state = value.lua_state();
        value.push();
        const lua_sf::detail::LuaSFNativeLookup lookup =
            lua_sf::detail::push_luasf_native_object<Element>(state, -1);
        if (lookup == lua_sf::detail::LuaSFNativeLookup::found ||
            lookup == lua_sf::detail::LuaSFNativeLookup::external) {
            result =
                lua_sf::detail::get_pushed_luasf_native_object<Element>(state);
            lua_pop(state, 2);
            return result != nullptr;
        }
        lua_pop(state, 1);
    }
    if (value.is<Native>()) {
        result = value.as<Native>();
        return true;
    }
    if (value.get_type() != sol::type::userdata) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push();
    if (lua_getiuservalue(state, -1, 1) != LUA_TTABLE) {
        lua_pop(state, 2);
        return false;
    }
    lua_pushliteral(state, "__nativeObjects");
    lua_rawget(state, -2);
    if (lua_type(state, -1) != LUA_TTABLE) {
        lua_pop(state, 3);
        return false;
    }
    sol::table nativeObjects = sol::stack::get<sol::table>(state, -1);
    lua_pop(state, 3);
    for (const auto& entry : nativeObjects) {
        if (!entry.second.is<Native>()) {
            continue;
        }
        result = entry.second.as<Native>();
        return true;
    }
    return false;
}

inline bool isLuaCompositeValue(const sol::object& value);

template <typename Pointer>
struct LuaCompositeSharedLifetime {
    LuaCompositeSharedLifetime(Pointer value,
                               ludork::standard::LuaRegistryReference reference)
        : value(std::move(value)), reference(std::move(reference)) {}

    Pointer value;
    ludork::standard::LuaRegistryReference reference;
};

template <typename Pointer>
bool tryReadSharedPointer(const sol::object& value, Pointer& result) {
    static_assert(IsSharedPointer<Pointer>::value);
    if (isNil(value)) {
        result = Pointer{};
        return true;
    }
    const bool composite = isLuaCompositeValue(value);
    ludork::standard::LuaRegistryReference compositeReference =
        composite ? makeLuaRegistryReference(value)
                  : ludork::standard::LuaRegistryReference{};
    if (tryReadNativeValue(value, result)) {
        sol::state_view lua(value.lua_state());
        const sol::object currentOwner = nativePointerOwner(lua, result.get());
        if (isNil(currentOwner)) {
            registerNativePointerOwner(lua, result.get(), value);
        }
        if (composite) {
            using Lifetime = LuaCompositeSharedLifetime<Pointer>;
            const std::shared_ptr<Lifetime> lifetime =
                std::make_shared<Lifetime>(result,
                                           std::move(compositeReference));
            result = Pointer(lifetime, lifetime->value.get());
        }
        return true;
    }
    return false;
}

template <typename Pointer>
Pointer readSharedPointer(const sol::object& value) {
    Pointer result;
    if (tryReadSharedPointer(value, result)) {
        return result;
    }
    throw std::invalid_argument(
        "Lua value is not compatible with the requested shared pointer");
}

template <typename Pointer>
bool tryReadPointer(const sol::object& value, Pointer& result) {
    static_assert(std::is_pointer_v<Pointer>);
    if (isNil(value)) {
        result = nullptr;
        return true;
    }
    return tryReadNativeValue(value, result);
}

template <typename Pointer>
Pointer readPointer(const sol::object& value) {
    Pointer result = nullptr;
    if (tryReadPointer(value, result)) {
        return result;
    }
    throw std::invalid_argument(
        "Lua value is not compatible with the requested pointer");
}

inline bool luaValueHasMetatable(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const bool result = lua_getmetatable(state, -1) != 0;
    if (result) {
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    return result;
}

inline bool isLuaCompositeValue(const sol::object& value) {
    if (value.get_type() != sol::type::userdata) {
        return false;
    }
    lua_State* state = value.lua_state();
    value.push();
    if (lua_getmetatable(state, -1) == 0) {
        lua_pop(state, 1);
        return false;
    }
    lua_getfield(state, -1, "__LuaSFNativeComposite");
    const bool result = lua_toboolean(state, -1) != 0;
    lua_pop(state, 3);
    return result;
}

inline const void* luaValueIdentity(const sol::object& value) {
    lua_State* state = value.lua_state();
    value.push();
    const void* result = lua_topointer(state, -1);
    lua_pop(state, 1);
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

template <typename DynamicValue>
bool dynamicValueHasCycle(const sol::object& value,
                          std::unordered_set<const void*>& activeTables,
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
        if (dynamicValueHasCycle<DynamicValue>(entry.second, activeTables,
                                               visitedTables)) {
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
DynamicValue readDynamicValue(const sol::object& value) {
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
    std::unordered_set<const void*> activeTables;
    std::unordered_set<const void*> visitedTables;
    if (dynamicValueHasCycle<DynamicValue>(value, activeTables,
                                           visitedTables)) {
        return makeOpaqueDynamicValue<DynamicValue>(value);
    }
    const sol::table table = value.as<sol::table>();
    std::size_t length = 0;
    if (dynamicTableIsArray<DynamicValue>(table, length)) {
        Array result;
        result.reserve(length);
        for (std::size_t index = 1; index <= length; ++index) {
            result.push_back(readDynamicValue<DynamicValue>(
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
                       readDynamicValue<DynamicValue>(entry.second));
    }
    return DynamicValue(std::move(result));
}

template <typename DynamicValue>
sol::object writeDynamicValue(sol::state_view lua, const DynamicValue& value) {
    using Storage = typename DynamicValue::Storage;
    using Object = typename DynamicValue::Object;
    return std::visit(
        [lua](const auto& item) -> sol::object {
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
        },
        static_cast<const Storage&>(value.storage()));
}

template <typename Signature>
struct LuaFunctionAdapter;

template <typename Return, typename... Arguments>
struct LuaFunctionAdapter<Return(Arguments...)> {
    static std::function<Return(Arguments...)> read(const sol::object& value) {
        if (isNil(value)) {
            return {};
        }
        if (!value.is<sol::protected_function>()) {
            throw std::invalid_argument("expected a Lua function");
        }
        const ludork::standard::LuaRegistryReference callbackReference =
            makeLuaRegistryReference(value);
        return [callbackReference](Arguments... arguments) mutable -> Return {
            lua_State* state = callbackReference.state();
            ludork::standard::LuaExecutionScope execution(state);
            if (!execution.active()) {
                throw std::runtime_error("Lua runtime session is stopping");
            }
            if (!callbackReference.push()) {
                throw std::runtime_error("Lua callback is no longer available");
            }
            if constexpr (std::is_void_v<Return>) {
                callPushedLuaFunction<void>(state, arguments...);
                return;
            } else {
                return callPushedLuaFunction<Return>(state, arguments...);
            }
        };
    }

    static sol::object write(
        sol::state_view lua,
        const std::function<Return(Arguments...)>& function) {
        if (!function) {
            return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
        }
        lua_State* state = lua.lua_state();
        auto wrapper = [state,
                        function](sol::variadic_args arguments) -> sol::object {
            ludork::standard::LuaExecutionScope execution(state);
            if (!execution.active()) {
                throw std::runtime_error("Lua runtime session is stopping");
            }
            if (arguments.size() != sizeof...(Arguments)) {
                throw std::invalid_argument(
                    "Lua callable argument count mismatch");
            }
            return invoke(sol::state_view(state), function, arguments,
                          std::index_sequence_for<Arguments...>{});
        };
        return sol::make_object(lua, sol::as_function(std::move(wrapper)));
    }

private:
    template <std::size_t... Index>
    static sol::object invoke(
        sol::state_view lua,
        const std::function<Return(Arguments...)>& function,
        const sol::variadic_args& arguments, std::index_sequence<Index...>) {
        std::tuple<LuaValueType<Arguments>...> values{
            readLuaValue<LuaValueType<Arguments>>(
                arguments.get<sol::object>(Index))...};
        if constexpr (std::is_void_v<Return>) {
            std::invoke(function,
                        static_cast<Arguments>(std::get<Index>(values))...);
            return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
        } else {
            return writeLuaValue(
                lua, std::invoke(function, static_cast<Arguments>(
                                               std::get<Index>(values))...));
        }
    }
};

template <>
struct LuaFunctionAdapter<void(const float*, unsigned int&, float*,
                               unsigned int&, unsigned int)> {
    using Function = std::function<void(const float*, unsigned int&, float*,
                                        unsigned int&, unsigned int)>;

    static Function read(const sol::object& value) {
        if (isNil(value)) {
            return {};
        }
        if (!value.is<sol::protected_function>()) {
            throw std::invalid_argument("expected a Lua function");
        }
        const ludork::standard::LuaRegistryReference callbackReference =
            makeLuaRegistryReference(value);
        return [callbackReference](
                   const float* inputFrames, unsigned int& inputFrameCount,
                   float* outputFrames, unsigned int& outputFrameCount,
                   unsigned int frameChannelCount) noexcept {
            const unsigned int initialInputFrameCount = inputFrameCount;
            const unsigned int initialOutputFrameCount = outputFrameCount;
            try {
                lua_State* state = callbackReference.state();
                ludork::standard::LuaExecutionScope execution(state);
                if (!execution.active()) {
                    throw std::runtime_error("Lua runtime session is stopping");
                }
                if (!callbackReference.push()) {
                    throw std::runtime_error(
                        "Lua callback is no longer available");
                }
                auto popper = sol::stack::pop_n(state, 1);
                sol::protected_function callback =
                    sol::stack::get<sol::protected_function>(state, -1);
                sol::state_view lua(state);
                const unsigned int inputFrameCapacity = inputFrameCount;
                const unsigned int outputFrameCapacity = outputFrameCount;
                sol::table input = lua_sf::audioFramesToTable(
                    lua, inputFrames, inputFrameCount, frameChannelCount);
                sol::table output = lua_sf::audioFramesToTable(
                    lua, outputFrames, outputFrameCount, frameChannelCount);

                sol::protected_function_result result =
                    callback(input, inputFrameCount, output, outputFrameCount,
                             frameChannelCount);
                lua_sf::throw_on_lua_error(result);

                sol::object outputValue = output;
                const sol::object returned = result;
                if (!isNil(returned)) {
                    if (returned.get_type() == sol::type::table) {
                        const sol::table table = returned.as<sol::table>();
                        const sol::object inputCountValue =
                            table.raw_get<sol::object>("inputFrameCount");
                        const sol::object outputCountValue =
                            table.raw_get<sol::object>("outputFrameCount");
                        const sol::object returnedOutputValue =
                            table.raw_get<sol::object>("output");
                        lua_sf::updateAudioFrameCount(inputCountValue,
                                                      inputFrameCount,
                                                      inputFrameCapacity);
                        lua_sf::updateAudioFrameCount(outputCountValue,
                                                      outputFrameCount,
                                                      outputFrameCapacity);
                        const bool hasNamedReturn = !isNil(inputCountValue) ||
                                                    !isNil(outputCountValue) ||
                                                    !isNil(returnedOutputValue);
                        if (!isNil(returnedOutputValue)) {
                            outputValue = returnedOutputValue;
                        } else if (!hasNamedReturn) {
                            outputValue = returned;
                        }
                    } else if (returned.get_type() == sol::type::number) {
                        lua_sf::updateAudioFrameCount(
                            returned, outputFrameCount, outputFrameCapacity);
                    }
                }
                lua_sf::copyAudioFramesFromObject(outputValue, outputFrames,
                                                  outputFrameCount,
                                                  frameChannelCount);
            } catch (...) {
                inputFrameCount = initialInputFrameCount;
                outputFrameCount = initialOutputFrameCount;
                lua_sf::passthroughAudioFrames(inputFrames, inputFrameCount,
                                               outputFrames, outputFrameCount,
                                               frameChannelCount);
            }
        };
    }

    static sol::object write(sol::state_view lua, const Function& function) {
        if (!function) {
            return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
        }
        auto wrapper =
            [function](sol::object inputValue,
                       lua_sf::LuaIntegral<unsigned int> inputCountValue,
                       sol::object outputValue,
                       lua_sf::LuaIntegral<unsigned int> outputCountValue,
                       lua_sf::LuaIntegral<unsigned int> channelCountValue) {
                ludork::standard::LuaExecutionScope execution(
                    inputValue.lua_state());
                if (!execution.active()) {
                    throw std::runtime_error("Lua runtime session is stopping");
                }
                unsigned int inputCount = inputCountValue.value();
                unsigned int outputCount = outputCountValue.value();
                const unsigned int channelCount = channelCountValue.value();
                sol::state_view callbackLua(inputValue.lua_state());
                sol::table result = callbackLua.create_table(0, 3);
                std::vector<float> inputFrames(
                    static_cast<std::size_t>(inputCount) * channelCount);
                std::vector<float> outputFrames(
                    static_cast<std::size_t>(outputCount) * channelCount);
                lua_sf::copyAudioFramesFromObject(
                    inputValue, inputFrames.data(), inputCount, channelCount);
                lua_sf::copyAudioFramesFromObject(outputValue,
                                                  outputFrames.data(),
                                                  outputCount, channelCount);
                function(inputFrames.data(), inputCount, outputFrames.data(),
                         outputCount, channelCount);
                result.raw_set("inputFrameCount", inputCount);
                result.raw_set("outputFrameCount", outputCount);
                result.raw_set("output", lua_sf::audioFramesToTable(
                                             callbackLua, outputFrames.data(),
                                             outputCount, channelCount));
                return result;
            };
        return sol::make_object(lua, sol::as_function(std::move(wrapper)));
    }
};

template <typename Signature>
std::function<Signature> functionFromLua(const sol::object& value) {
    return LuaFunctionAdapter<Signature>::read(value);
}

template <typename Signature>
sol::object functionToLua(sol::state_view lua,
                          const std::function<Signature>& value) {
    return LuaFunctionAdapter<Signature>::write(lua, value);
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
    sol::table table = lua.create_table(static_cast<int>(value.size()), 0);
    using Item = typename Sequence::value_type;
    if constexpr (IsOptionalValue<Item> || IsDynamicValue<Item>) {
        table.raw_set("n", value.size());
    }
    std::size_t index = 1;
    for (const auto& item : value) {
        sol::object output;
        if constexpr (std::is_same_v<Item, bool>) {
            const bool converted = static_cast<bool>(item);
            output = writeLuaValue(lua, converted);
        } else {
            output = writeLuaValue(lua, item);
        }
        if (!isNil(output)) {
            table.raw_set(index, output);
        }
        ++index;
    }
    return sol::make_object(lua, table);
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

template <typename T>
sol::object writeLuaCallbackArgument(sol::state_view lua, T&& value) {
    using Value = LuaValueType<T>;
    constexpr bool converted =
        IsDynamicValue<Value> || IsTableValue<Value> ||
        IsOpaqueIdentityPointer<Value>::value || IsStdFunction<Value> ||
        IsVector<Value>::value || IsArray<Value>::value ||
        IsPair<Value>::value || IsMap<Value>::value ||
        IsOptional<Value>::value || IsVariant<Value>::value ||
        IsSharedPointer<Value>::value || std::is_pointer_v<Value> ||
        std::is_same_v<Value, std::string> ||
        std::is_same_v<Value, sol::object>;
    if constexpr (converted) {
        return writeLuaValue(lua, value);
    } else if constexpr (std::is_lvalue_reference_v<T> &&
                         std::is_class_v<Value>) {
        const sol::object owner =
            nativePointerOwner(lua, std::addressof(value));
        if (!isNil(owner)) {
            return owner;
        }
        using Reference = std::remove_reference_t<T>;
        if constexpr (std::is_const_v<Reference>) {
            return sol::make_object(lua, std::cref(value));
        } else {
            return sol::make_object(lua, std::ref(value));
        }
    } else {
        return writeLuaValue(lua, value);
    }
}

template <typename Return, typename... Arguments>
Return callPushedLuaFunction(lua_State* state, Arguments&&... arguments) {
    const int stackBase = lua_gettop(state) - 1;
    try {
        sol::state_view lua(state);
        (writeLuaCallbackArgument(lua, std::forward<Arguments>(arguments))
             .push(),
         ...);
        constexpr int resultCount = std::is_void_v<Return> ? 0 : 1;
        if (ludork::standard::protectedLuaCall(state, sizeof...(Arguments),
                                               resultCount) != LUA_OK) {
            const std::string message =
                ludork::standard::luaErrorMessage(state, -1);
            throw std::runtime_error(message);
        }
        if constexpr (std::is_void_v<Return>) {
            lua_settop(state, stackBase);
            return;
        } else {
            const sol::object rawResult =
                sol::stack::get<sol::object>(state, -1);
            Return result = readLuaValue<Return>(rawResult);
            lua_settop(state, stackBase);
            return result;
        }
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

}  // namespace ludork_core
