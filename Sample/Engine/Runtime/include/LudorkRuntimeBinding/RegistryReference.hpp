#pragma once

#include <RuntimeSession.hpp>
#include <utils.hpp>

#include <stdexcept>
#include <utility>

namespace ludork::runtime::binding {

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
    if (!reference.pushUnderExecutionScope(state)) {
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

}  // namespace ludork::runtime::binding
