#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>

namespace ludork::runtime::graphics {

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct LUDORK_RUNTIME_API EmitterBurst {
    BIND_PROPERTY()
    float time = 0;
    BIND_PROPERTY()
    int count = 0;
    BIND_PROPERTY()
    int cycles = 1;
    BIND_PROPERTY()
    float interval = 0;
};

}  // namespace ludork::runtime::graphics
