#pragma once

#include <Runtime/AssetStore.hpp>
#include "LdPakArchive.hpp"

#include <cstdint>
#include <filesystem>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace ludork::runtime::asset_store_impl {

struct StoreEntry {
    std::filesystem::path source;
    std::uint64_t size = 0;
    double modificationTime = 0.0;
    bool directory = false;
    std::shared_ptr<const detail::LdPakArchive> archive;
    std::string archivePath;
};

}  // namespace ludork::runtime::asset_store_impl

namespace ludork::runtime {

struct AssetStore::Impl {
    mutable std::shared_mutex mutex;
    std::filesystem::path runtimeRoot;
    AssetStoreMode mode = AssetStoreMode::Loose;
    bool configured = false;
    std::unordered_map<std::string, asset_store_impl::StoreEntry> entries;
};

}  // namespace ludork::runtime
