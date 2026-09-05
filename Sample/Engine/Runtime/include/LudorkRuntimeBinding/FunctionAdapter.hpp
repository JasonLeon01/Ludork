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
            if (!callbackReference.pushUnderExecutionScope()) {
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

}  // namespace ludork::runtime::binding
