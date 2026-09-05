#include "WorldFogRuntime.hpp"

namespace ludork::global::fog_controller_impl {

WorldFogRuntime& worldFogRuntime() {
    static WorldFogRuntime runtime;
    return runtime;
}

}  // namespace ludork::global::fog_controller_impl
