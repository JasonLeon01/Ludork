#pragma once
#include <Runtime/AssetInputStream.hpp>

#include <Runtime/AssetStore.hpp>

#include <cstdint>
#include <fstream>

namespace ludork::runtime {

struct AssetInputStream::Impl {
    std::ifstream stream;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint64_t position = 0;
};

}  // namespace ludork::runtime
