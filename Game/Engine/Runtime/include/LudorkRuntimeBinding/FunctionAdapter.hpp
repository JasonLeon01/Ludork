#pragma once

#include <LuaError.hpp>
#include <LudorkRuntimeBinding/NativeObjectCodec.hpp>
#include <LudorkRuntimeBinding/RegistryReference.hpp>

#include <functional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ludork::runtime::binding {

template <typename Return, bool Exact, typename... Arguments>
Return callPushedLuaFunctionWithPolicy(lua_State* state,
                                       Arguments&&... arguments);

template <typename Signature>
struct LuaFunctionAdapter;

template <typename Return, typename... Arguments>
struct LuaFunctionAdapter<Return(Arguments...)> {
    template <bool Exact = false>
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
            if (!callbackReference.pushUnderExecutionScope()) {
                throw std::runtime_error("Lua callback is no longer available");
            }
            if constexpr (std::is_void_v<Return>) {
                callPushedLuaFunctionWithPolicy<void, Exact>(state,
                                                             arguments...);
                return;
            } else {
                return callPushedLuaFunctionWithPolicy<Return, Exact>(
                    state, arguments...);
            }
        };
    }

    static sol::object write(
        sol::state_view lua,
        const std::function<Return(Arguments...)>& function) {
        if (!function) {
            return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
        }
        auto wrapper = [function](sol::this_state currentState,
                                  sol::variadic_args arguments) -> sol::object {
            lua_State* state = currentState;
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

template <typename Signature>
std::function<Signature> functionFromLua(const sol::object& value) {
    return LuaFunctionAdapter<Signature>::read(value);
}

template <typename Signature>
sol::object functionToLua(sol::state_view lua,
                          const std::function<Signature>& value) {
    return LuaFunctionAdapter<Signature>::write(lua, value);
}

template <typename T>
sol::object writeLuaCallbackArgument(sol::state_view lua, T&& value) {
    using Value = LuaValueType<T>;
    constexpr bool converted =
        IsDynamicValue<Value> || IsPureDataValue<Value> ||
        IsTableValue<Value> || IsOpaqueIdentityPointer<Value>::value ||
        IsStdFunction<Value> || IsVector<Value>::value ||
        IsArray<Value>::value || IsPair<Value>::value || IsMap<Value>::value ||
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

template <typename Return, bool Exact, typename... Arguments>
Return callPushedLuaFunctionWithPolicy(lua_State* state,
                                       Arguments&&... arguments) {
    const int stackBase = lua_gettop(state) - 1;
    try {
        sol::state_view lua(state);
        (writeLuaCallbackArgument(lua, std::forward<Arguments>(arguments))
             .push(),
         ...);
        constexpr int resultCount =
            Exact ? LUA_MULTRET : (std::is_void_v<Return> ? 0 : 1);
        if (ludork::standard::protectedLuaCall(state, sizeof...(Arguments),
                                               resultCount) != LUA_OK) {
            const std::string message =
                ludork::standard::luaErrorMessage(state, -1);
            throw std::runtime_error(message);
        }
        if constexpr (Exact) {
            constexpr int expected = std::is_void_v<Return> ? 0 : 1;
            if (lua_gettop(state) - stackBase != expected) {
                throw std::invalid_argument(
                    "Lua callback must return exactly " +
                    std::to_string(expected) + " value(s)");
            }
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

template <typename Return, typename... Arguments>
Return callPushedLuaFunction(lua_State* state, Arguments&&... arguments) {
    return callPushedLuaFunctionWithPolicy<Return, false>(
        state, std::forward<Arguments>(arguments)...);
}

template <typename Signature>
ludork::runtime::StrictFunction<Signature> strictFunctionFromLua(
    const sol::object& value) {
    if (isNil(value)) {
        return {};
    }
    using Function = ludork::runtime::StrictFunction<Signature>;
    struct Reference final : Function::Reference {
        ludork::standard::LuaRegistryReference value;
        explicit Reference(ludork::standard::LuaRegistryReference reference)
            : value(std::move(reference)) {}
        bool push(lua_State* state) const override {
            return value.state() == state && value.pushUnderExecutionScope();
        }
    };
    return Function(
        LuaFunctionAdapter<Signature>::template read<true>(value),
        std::make_shared<Reference>(makeLuaRegistryReference(value)));
}

template <typename Signature>
sol::object strictFunctionToLua(
    sol::state_view lua,
    const ludork::runtime::StrictFunction<Signature>& value) {
    if (!value) {
        return sol::make_object(lua, lua_sf::LUASF_SOL_NIL);
    }
    if (!value.reference()) {
        return functionToLua(lua, value.function());
    }
    ludork::standard::LuaExecutionScope execution(lua.lua_state());
    if (!execution.active() || !value.reference()->push(lua.lua_state())) {
        throw std::runtime_error(
            "Lua callback belongs to an unavailable session");
    }
    sol::object result = sol::stack::get<sol::object>(lua.lua_state(), -1);
    lua_pop(lua.lua_state(), 1);
    return result;
}

}  // namespace ludork::runtime::binding
