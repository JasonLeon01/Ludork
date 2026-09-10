#include "PanoramaControllerImpl.hpp"

namespace ludork::global::panorama_controller_impl {

PanoramaControllerImpl& panoramaControllerImpl() {
    static PanoramaControllerImpl impl;
    return impl;
}

}  // namespace ludork::global::panorama_controller_impl
