#include "SystemImpl.hpp"

namespace ludork::global::system_impl {

SystemImpl& impl() {
    static SystemImpl state;
    return state;
}

}  // namespace ludork::global::system_impl
