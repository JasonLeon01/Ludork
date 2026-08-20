#pragma once

#include <RuntimeSession.hpp>

#include <sol2/forward.hpp>

#include <optional>

struct lua_State;

namespace ludork::engine::runtime_detail {

void installEngineRuntimeState(lua_State* state);
void clearEngineRuntimeState(lua_State* state) noexcept;

class EngineRuntimeScope {
public:
    EngineRuntimeScope();

    sol::state_view lua() const;
    lua_State* state() const noexcept;

private:
    lua_State* state_ = nullptr;
    std::optional<ludork::standard::LuaExecutionScope> execution_;
};

}  // namespace ludork::engine::runtime_detail
