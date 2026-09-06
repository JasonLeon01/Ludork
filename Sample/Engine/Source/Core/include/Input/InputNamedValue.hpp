#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>

BIND_CLASS(copyable = true, table_init = true, metadata = false,
           lua_alternatives = "integer=>value=$", lua_tostring = "name")
struct InputNamedValue {
    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    int value = 0;
};
