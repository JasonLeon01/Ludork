#pragma once

#include <Runtime/ScriptStore.hpp>

#include "LdPakArchive.hpp"

#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ludork::runtime::script_store_impl {

struct ScriptEntry {
    std::filesystem::path source;
    std::string archivePath;
};

}  // namespace ludork::runtime::script_store_impl

namespace ludork::runtime {

struct ScriptStore::Impl {
    mutable std::shared_mutex mutex;
    std::filesystem::path runtimeRoot;
    ScriptStoreMode mode = ScriptStoreMode::Loose;
    bool configured = false;
    std::uint64_t generation = 0;
    const ScriptStore* reloadOwner = nullptr;
    std::uint64_t reloadGeneration = 0;
    std::shared_ptr<detail::LdPakArchive> archive;
    std::unordered_map<std::string, script_store_impl::ScriptEntry> entries;
    std::unordered_map<std::string, std::string> modules;
    std::vector<std::string> orderedModules;
};

}  // namespace ludork::runtime
