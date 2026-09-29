#pragma once

#include "LdPakEntry.hpp"
#include <filesystem>
#include <fstream>
#include <limits>

namespace ludork::runtime::detail {

class LdPakEntryReader final {
public:
    LdPakEntryReader(const std::filesystem::path& source,
                     const LdPakEntry& entry);
    void read(std::uint64_t position, void* data, std::size_t size);
    [[nodiscard]] std::vector<std::uint8_t> readAll();

private:
    void loadBlock(std::size_t index);

    const LdPakEntry& entry_;
    std::ifstream stream_;
    std::vector<std::uint8_t> buffer_;
    std::size_t cachedBlock_ = std::numeric_limits<std::size_t>::max();
    std::uint64_t checkedPosition_ = 0;
    std::uint32_t checksum_ = 0;
};

}  // namespace ludork::runtime::detail
