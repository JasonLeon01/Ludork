#pragma once
#include <Runtime/AssetInputStream.hpp>

#include <Runtime/AssetStore.hpp>
#include "LdPakArchive.hpp"
#include "LdPakEntryReader.hpp"

#include <cstdint>
#include <fstream>

namespace ludork::runtime {

struct AssetInputStream::Impl {
    std::ifstream stream;
    std::shared_ptr<const detail::LdPakArchive> archive;
    std::unique_ptr<detail::LdPakEntryReader> reader;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint64_t position = 0;
};

}  // namespace ludork::runtime
