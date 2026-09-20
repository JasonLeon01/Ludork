#pragma once

#include <Cast.hpp>
#include <RuntimeSession.hpp>
#include <RuntimeApi.hpp>
#include <LuaGlue/LuaGlue.hpp>

#include <stdexcept>
#include <utility>

namespace ludork::runtime::binding {

class LUDORK_RUNTIME_API LuaRegistryReferenceOwner {
public:
    LUDORK_CAST_ROOT(LuaRegistryReferenceOwner)

    virtual ~LuaRegistryReferenceOwner();
    virtual const ludork::standard::LuaRegistryReference& registryReference()
        const noexcept = 0;
};

inline ludork::standard::LuaRegistryReference makeLuaRegistryReference(
    const lua_glue::Object& value) {
    lua_State* state = value.lua_state();
    ludork::standard::LuaExecutionScope execution(state);
    if (!execution.active()) {
        throw std::runtime_error("Lua runtime session is stopping");
    }
    auto pushed = lua_glue::PushGuard(value);
    return ludork::standard::LuaRegistryReference(state, pushed.index());
}

inline ludork::standard::LuaRegistryReference makeLuaCallbackReference(
    const lua_glue::Table& callbacks, const char* name) {
    const lua_glue::Object callback = callbacks.raw_get<lua_glue::Object>(name);
    if (!callback.is<lua_glue::Function>()) {
        return {};
    }
    return makeLuaRegistryReference(callback);
}

inline lua_glue::Object readLuaRegistryReference(
    lua_glue::StateView lua,
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
    auto popper = lua_glue::PopGuard(state, 1);
    return lua_glue::Read<lua_glue::Object>(state, -1);
}

template <typename Base>
class LuaOpaqueObject final : public Base, public LuaRegistryReferenceOwner {
public:
    LUDORK_CAST_DERIVED(LuaOpaqueObject, Base, LuaRegistryReferenceOwner)

    explicit LuaOpaqueObject(const lua_glue::Object& value)
        : value_(makeLuaRegistryReference(value)) {
        ludork::standard::registerRuntimeOpaqueValue(this, value_);
    }

    ~LuaOpaqueObject() {
        ludork::standard::unregisterRuntimeOpaqueValue(value_.state(), this);
    }

    const ludork::standard::LuaRegistryReference& registryReference()
        const noexcept override {
        return value_;
    }

private:
    ludork::standard::LuaRegistryReference value_;
};

template <typename Base>
class LuaOpaqueIdentity final : public Base, public LuaRegistryReferenceOwner {
public:
    LUDORK_CAST_DERIVED(LuaOpaqueIdentity, Base, LuaRegistryReferenceOwner)

    explicit LuaOpaqueIdentity(const lua_glue::Object& value)
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

    const ludork::standard::LuaRegistryReference& registryReference()
        const noexcept override {
        return value_;
    }

private:
    ludork::standard::LuaRegistryReference value_;
};

}  // namespace ludork::runtime::binding
