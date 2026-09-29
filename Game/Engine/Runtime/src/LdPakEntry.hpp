#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ludork::runtime::detail {

struct LdPakEntry {
    struct Block {
        std::uint64_t offset = 0;
        std::uint32_t storedSize = 0;
        std::uint32_t flags = 0;
        std::uint32_t crc = 0;
    };

    std::string path;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint64_t storedSize = 0;
    std::uint32_t crc = 0;
    bool directory = false;
    std::vector<Block> blocks;
};

}  // namespace ludork::runtime::detail
