#include "ResourceFileInfo.hpp"

#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>

namespace ludork::runtime::detail {

double resourceModificationTime(const std::filesystem::path& path,
                                std::string_view failurePrefix) {
    std::error_code error;
    const std::filesystem::file_time_type writeTime =
        std::filesystem::last_write_time(path, error);
    if (error) {
        throw std::runtime_error(std::string(failurePrefix) + error.message());
    }
    struct ClockCalibration {
        std::filesystem::file_time_type fileTime;
        std::chrono::system_clock::time_point systemTime;
    };
    static const ClockCalibration calibration{
        std::filesystem::file_time_type::clock::now(),
        std::chrono::system_clock::now()};
    const std::chrono::system_clock::time_point systemTime =
        calibration.systemTime +
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            writeTime - calibration.fileTime);
    return std::chrono::duration<double>(systemTime.time_since_epoch()).count();
}

std::uint64_t resourceFileSize(const std::filesystem::path& path,
                               std::string_view failurePrefix,
                               std::string_view overflowMessage) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        throw std::runtime_error(std::string(failurePrefix) + error.message());
    }
    if (size > std::numeric_limits<std::uint64_t>::max()) {
        throw std::runtime_error(std::string(overflowMessage));
    }
    return static_cast<std::uint64_t>(size);
}

}  // namespace ludork::runtime::detail
