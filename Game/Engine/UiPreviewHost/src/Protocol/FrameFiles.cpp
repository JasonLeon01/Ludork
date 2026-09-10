#include "Protocol/FrameFiles.hpp"

#include <Utf8Path.hpp>

#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace ludork::preview_host {

FrameFiles::FrameFiles() {
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path temporary =
        std::filesystem::temp_directory_path();
    for (std::size_t index = 0; index < paths_.size(); ++index) {
        paths_[index] =
            temporary / ("LudorkUiPreview-" + std::to_string(stamp) + "-" +
                         std::to_string(index) + ".bin");
    }
}

FrameFiles::~FrameFiles() {
    for (const std::filesystem::path& path : paths_) {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
}

const std::filesystem::path& FrameFiles::write(
    const std::vector<std::uint8_t>& pixels) {
    current_ = (current_ + 1) % paths_.size();
    const std::filesystem::path& path = paths_[current_];
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open preview frame buffer: " +
                                 ludork::standard::pathToUtf8(path));
    }
    output.write(reinterpret_cast<const char*>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));
    if (!output) {
        throw std::runtime_error("Failed to write preview frame buffer: " +
                                 ludork::standard::pathToUtf8(path));
    }
    return path;
}

}  // namespace ludork::preview_host
