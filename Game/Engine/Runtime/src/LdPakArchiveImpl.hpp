#pragma once

#include "LdPakArchive.hpp"
#include <unordered_map>

namespace ludork::runtime::detail {

struct LdPakArchive::Impl {
    std::filesystem::path path;
    std::string group;
    double modificationTime = 0.0;
    std::vector<LdPakEntry> entries;
    std::unordered_map<std::string, std::size_t> entryIndices;
};

}  // namespace ludork::runtime::detail
