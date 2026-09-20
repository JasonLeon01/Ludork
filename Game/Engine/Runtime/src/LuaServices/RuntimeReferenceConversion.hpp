#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <LuaGlue/LuaGlue.hpp>

namespace ludork::runtime::detail {

RuntimeValue readRuntimeReference(const lua_glue::Object& value);

}  // namespace ludork::runtime::detail
