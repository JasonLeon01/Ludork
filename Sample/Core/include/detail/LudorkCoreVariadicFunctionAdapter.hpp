#pragma once

#if !defined(LUDORK_CORE_BINDING_DETAIL_FRAGMENT)
#error "LudorkCoreVariadicFunctionAdapter.hpp must be included by LudorkCoreBinding.hpp"
#endif

template <typename Signature>
struct LuaVariadicFunctionAdapter;

template <typename Return, typename... Arguments>
struct LuaVariadicFunctionAdapter<Return(Arguments...)> {
    using ArgumentTuple = std::tuple<Arguments...>;
    using ReturnVector = LuaValueType<Return>;
    static constexpr std::size_t argumentCount = sizeof...(Arguments);
    static_assert(argumentCount != 0);
    using VariadicArgument =
        std::tuple_element_t<argumentCount - 1, ArgumentTuple>;
    using VariadicVector = LuaValueType<VariadicArgument>;
    static_assert(IsVector<ReturnVector>::value);
    static_assert(IsVector<VariadicVector>::value);
    static_assert(std::is_lvalue_reference_v<VariadicArgument>);
    static_assert(std::is_const_v<std::remove_reference_t<VariadicArgument>>);
    static_assert(std::is_same_v<typename ReturnVector::value_type,
                                 typename VariadicVector::value_type>);

    static std::function<Return(Arguments...)> read(const sol::object& value) {
        if (isNil(value)) {
            return {};
        }
        if (!value.is<sol::protected_function>()) {
            throw std::invalid_argument("expected a Lua function");
        }
        const ludork::standard::LuaRegistryReference callbackReference =
            makeLuaRegistryReference(value);
        return [callbackReference](Arguments... arguments) -> Return {
            lua_State* state = callbackReference.state();
            ludork::standard::LuaExecutionScope execution(state);
            if (!execution.active()) {
                throw std::runtime_error("Lua runtime session is stopping");
            }
            if (!callbackReference.pushUnderExecutionScope()) {
                throw std::runtime_error("Lua callback is no longer available");
            }
            auto values = std::forward_as_tuple(arguments...);
            return call(state, values,
                        std::make_index_sequence<argumentCount - 1>{});
        };
    }

private:
    template <typename Values, std::size_t... Index>
    static Return call(lua_State* state, Values& values,
                       std::index_sequence<Index...>) {
        const int stackBase = lua_gettop(state) - 1;
        try {
            sol::state_view lua(state);
            const VariadicVector& variadic =
                std::get<argumentCount - 1>(values);
            constexpr std::size_t fixedCount = sizeof...(Index);
            static_assert(fixedCount <=
                          static_cast<std::size_t>(INT_MAX - LUA_MINSTACK));
            if (variadic.size() >
                static_cast<std::size_t>(INT_MAX) - fixedCount -
                    LUA_MINSTACK) {
                throw std::length_error("Lua callable argument count overflow");
            }
            const std::size_t pushedCount = fixedCount + variadic.size();
            const std::size_t reservedCount = pushedCount + LUA_MINSTACK;
            if (lua_checkstack(state, static_cast<int>(reservedCount)) == 0) {
                throw std::runtime_error(
                    "Lua stack cannot grow for callable arguments");
            }
            (writeLuaValue(lua, std::get<Index>(values)).push(), ...);
            for (const typename VariadicVector::value_type& value : variadic) {
                writeLuaValue(lua, value).push();
            }
            const int status = ludork::standard::protectedLuaCall(
                state, static_cast<int>(pushedCount), LUA_MULTRET);
            if (lua_checkstack(state, LUA_MINSTACK) == 0) {
                throw std::runtime_error(
                    "Lua stack cannot grow for callable results");
            }
            if (status != LUA_OK) {
                throw std::runtime_error(
                    ludork::standard::luaErrorMessage(state, -1));
            }
            const int resultCount = lua_gettop(state) - stackBase;
            Return result;
            result.reserve(static_cast<std::size_t>(resultCount));
            for (int index = stackBase + 1; index <= lua_gettop(state);
                 ++index) {
                result.push_back(readLuaValue<typename ReturnVector::value_type>(
                    sol::stack::get<sol::object>(state, index)));
            }
            lua_settop(state, stackBase);
            return result;
        } catch (...) {
            lua_settop(state, stackBase);
            throw;
        }
    }
};

template <typename Signature>
std::function<Signature> variadicFunctionFromLua(const sol::object& value) {
    return LuaVariadicFunctionAdapter<Signature>::read(value);
}
