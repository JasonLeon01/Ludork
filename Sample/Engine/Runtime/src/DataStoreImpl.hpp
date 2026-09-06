#pragma once

#include <Runtime/DataStore.hpp>

#include "LdPakArchive.hpp"

#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ludork::runtime::data_store_impl {

struct StoreEntry {
    std::filesystem::path source;
    std::shared_ptr<detail::LdPakArchive> archive;
    std::string archivePath;
    DataStat stat;
};

}  // namespace ludork::runtime::data_store_impl

namespace ludork::runtime {

struct DataStore::Impl {
    mutable std::shared_mutex mutex;
    std::filesystem::path runtimeRoot;
    DataStoreMode mode = DataStoreMode::Loose;
    bool configured = false;
    std::unordered_map<std::string, data_store_impl::StoreEntry> entries;
    std::unordered_map<std::string, std::vector<std::filesystem::path>>
        directoryEntries;
};

}  // namespace ludork::runtime
