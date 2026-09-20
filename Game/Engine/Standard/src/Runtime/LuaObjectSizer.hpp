#pragma once

#include <LuaGlue/LuaGlue.hpp>

#include <cstdint>

namespace ludork::standard {

std::int64_t luaObjectSize(const lua_glue::Object& value);

}
