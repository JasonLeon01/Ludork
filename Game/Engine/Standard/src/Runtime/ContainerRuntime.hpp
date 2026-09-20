#pragma once

#include <LuaGlue/LuaGlue.hpp>

#include <cstddef>
#include <vector>

struct lua_State;

namespace ludork::standard::container_runtime {

void registerContainers(lua_glue::StateView lua);
void shutdownContainers(lua_State* state) noexcept;
bool containerLength(lua_State* state, int index, std::size_t& length);
bool isContainer(const lua_glue::Object& value);
std::size_t containerStorageSize(const lua_glue::Object& value);
std::vector<lua_glue::Object> containerChildren(const lua_glue::Object& value);

}  // namespace ludork::standard::container_runtime
