#pragma once

#include <LuaGlue/LuaGlue.hpp>

#include <string>

namespace ludork::runtime::detail {

struct RuntimeClassIdentity {
    lua_glue::Table descriptor;
    std::string module;
    std::string type;
    bool direct = false;
};

}  // namespace ludork::runtime::detail
