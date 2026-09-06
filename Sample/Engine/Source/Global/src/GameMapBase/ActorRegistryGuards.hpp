#pragma once

#include <cstddef>

namespace ludork::global::game_map_base_impl {

class BoolResetGuard {
public:
    explicit BoolResetGuard(bool& value);

    BoolResetGuard(const BoolResetGuard&) = delete;
    BoolResetGuard& operator=(const BoolResetGuard&) = delete;

    ~BoolResetGuard();

private:
    bool& value_;
};

class DepthGuard {
public:
    explicit DepthGuard(std::size_t& depth);

    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;

    ~DepthGuard();

private:
    std::size_t& depth_;
};

}  // namespace ludork::global::game_map_base_impl
