#pragma once

#include <string>

struct lua_State;

namespace ludork::runtime {

void captureScriptModuleShape(lua_State* state, const std::string& name,
                              int definition);
void validateScriptModuleShape(lua_State* state, const std::string& name,
                               int candidate);

}  // namespace ludork::runtime
