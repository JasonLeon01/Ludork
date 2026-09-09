#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace ludork::preview_host {

class FrameFiles {
public:
    FrameFiles();
    ~FrameFiles();

    FrameFiles(const FrameFiles&) = delete;
    FrameFiles& operator=(const FrameFiles&) = delete;
    FrameFiles(FrameFiles&&) = delete;
    FrameFiles& operator=(FrameFiles&&) = delete;

    const std::filesystem::path& write(const std::vector<std::uint8_t>& pixels);

private:
    std::array<std::filesystem::path, 2> paths_;
    std::size_t current_ = 0;
};

}  // namespace ludork::preview_host
