#include "FogRenderRuntime.hpp"

namespace ludork::global::fog_controller_impl {

FogRenderRuntime& fogRenderRuntime() {
    static FogRenderRuntime runtime;
    return runtime;
}

}  // namespace ludork::global::fog_controller_impl
