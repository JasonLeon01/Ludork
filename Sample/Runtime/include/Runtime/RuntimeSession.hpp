#pragma once

#include <RuntimeApi.hpp>
#include <RuntimeSession.hpp>

#include <sol2/forward.hpp>

#include <optional>

struct lua_State;

namespace ludork::runtime {

LUDORK_RUNTIME_API void initialize(lua_State* state);
LUDORK_RUNTIME_API void shutdown(lua_State* state) noexcept;

class LUDORK_RUNTIME_API RuntimeScope {
public:
    RuntimeScope();

    sol::state_view lua() const;
    lua_State* state() const noexcept;

private:
    lua_State* state_ = nullptr;
    std::optional<ludork::standard::LuaExecutionScope> execution_;
};

}  // namespace ludork::runtime
