#include "ResourceStorePaths.hpp"

#include <stdexcept>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ludork::runtime::detail {

std::filesystem::path resourceStoreRoot(
    const std::filesystem::path& runtimeRoot, const std::string& name,
    const bool packed) {
    std::error_code error;
    const std::filesystem::path normalized =
        std::filesystem::weakly_canonical(runtimeRoot, error);
    if (error || normalized.empty()) {
        throw std::invalid_argument("Invalid runtime root for " + name);
    }
    const std::filesystem::path selected =
        normalized / (name + (packed ? ".ldpak" : ""));
    const std::filesystem::path alternate =
        normalized / (name + (packed ? "" : ".ldpak"));
    const std::filesystem::file_status alternateStatus =
        std::filesystem::symlink_status(alternate, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("Failed to inspect " + name + ": " +
                                 error.message());
    }
    if (std::filesystem::exists(alternateStatus)) {
        throw std::runtime_error("Runtime root must contain only " + name +
                                 (packed ? ".ldpak" : "") +
                                 " for the selected resource layout");
    }
    error.clear();
    const std::filesystem::file_status status =
        std::filesystem::symlink_status(selected, error);
    if (error || (packed ? !std::filesystem::is_regular_file(status)
                         : !std::filesystem::is_directory(status))) {
        throw std::runtime_error("Runtime root must contain " + name +
                                 (packed ? ".ldpak" : ""));
    }
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(selected.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        throw std::runtime_error(name + " must not be a filesystem link");
    }
#endif
    return normalized;
}

}  // namespace ludork::runtime::detail
