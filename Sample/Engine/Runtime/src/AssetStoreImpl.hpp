#pragma once

#include <Runtime/AssetStore.hpp>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ludork::runtime::asset_store_impl {

struct StoreEntry {
    std::filesystem::path source;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint32_t crc = 0;
    double modificationTime = 0.0;
    bool directory = false;
    bool packed = false;
};

}  // namespace ludork::runtime::asset_store_impl

namespace ludork::runtime {

struct AssetStore::Impl {
    mutable std::shared_mutex mutex;
    mutable std::mutex validationMutex;
    std::filesystem::path runtimeRoot;
    AssetStoreMode mode = AssetStoreMode::Loose;
    bool configured = false;
    std::unordered_map<std::string, asset_store_impl::StoreEntry> entries;
    mutable std::unordered_set<std::string> validatedEntries;
};

}  // namespace ludork::runtime
