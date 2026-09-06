#include "FogRenderImpl.hpp"

namespace ludork::global::fog_controller_impl {

FogRenderImpl& fogRenderImpl() {
    static FogRenderImpl impl;
    return impl;
}

}  // namespace ludork::global::fog_controller_impl
