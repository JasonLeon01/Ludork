#include "WorldFogImpl.hpp"

namespace ludork::global::fog_controller_impl {

WorldFogImpl& worldFogImpl() {
    static WorldFogImpl impl;
    return impl;
}

}  // namespace ludork::global::fog_controller_impl
