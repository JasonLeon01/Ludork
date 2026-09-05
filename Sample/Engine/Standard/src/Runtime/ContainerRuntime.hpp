#pragma once

#include <sol2/forward.hpp>

#include <cstddef>
#include <vector>

struct lua_State;

namespace ludork::standard::container_runtime {

void registerContainers(sol::state_view lua);
void shutdownContainers(lua_State* state) noexcept;
bool containerLength(lua_State* state, int index, std::size_t& length);
bool isContainer(const sol::object& value);
std::size_t containerStorageSize(const sol::object& value);
std::vector<sol::object> containerChildren(const sol::object& value);

}  // namespace ludork::standard::container_runtime
