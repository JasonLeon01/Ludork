#include "ActorRegistryGuards.hpp"

namespace ludork::global::game_map_base_impl {

BoolResetGuard::BoolResetGuard(bool& value) : value_(value) {
    value_ = true;
}

BoolResetGuard::~BoolResetGuard() {
    value_ = false;
}

DepthGuard::DepthGuard(std::size_t& depth) : depth_(depth) {
    ++depth_;
}

DepthGuard::~DepthGuard() {
    --depth_;
}

}  // namespace ludork::global::game_map_base_impl
