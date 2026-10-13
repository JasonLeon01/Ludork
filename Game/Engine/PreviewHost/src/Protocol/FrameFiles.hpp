#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
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
    std::span<std::uint8_t> acquire(std::size_t byteCount);
    const std::filesystem::path& path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::preview_host
