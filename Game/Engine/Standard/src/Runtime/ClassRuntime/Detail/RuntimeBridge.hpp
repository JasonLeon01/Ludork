#pragma once

#include <LuaGlue/LuaGlue.hpp>

namespace ludork::standard::class_runtime::detail {

lua_glue::Table resolverMro(lua_glue::StateView lua,
                            const lua_glue::Table& classTable);

}  // namespace ludork::standard::class_runtime::detail
