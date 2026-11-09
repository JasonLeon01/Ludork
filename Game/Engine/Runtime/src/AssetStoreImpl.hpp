#pragma once

#include <Runtime/AssetStore.hpp>
#include "LdPakArchive.hpp"

#include <cstdint>
#include <filesystem>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>

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
    std::filesystem::path userDataRoot;
    AssetStoreMode mode = AssetStoreMode::Loose;
    bool configured = false;
    std::unordered_map<std::string, asset_store_impl::StoreEntry> entries;

    std::optional<std::pair<std::string, asset_store_impl::StoreEntry>> resolve(
        const std::vector<std::string>& candidatePaths) const;
};

}  // namespace ludork::runtime
