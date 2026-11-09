#include "ResourceStorePaths.hpp"
#include <LudorkGenerated/ResourceFileConstants.hpp>
#include <Utf8Path.hpp>

#include <stdexcept>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ludork::runtime::detail {

std::string asciiFold(std::string value) {
    for (char& character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return value;
}

bool isLinkLike(const std::filesystem::path& path,
                const std::filesystem::file_status& status,
                std::string_view inspectionError) {
    if (std::filesystem::is_symlink(status)) {
        return true;
    }
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        throw std::runtime_error(std::string(inspectionError));
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    static_cast<void>(path);
    static_cast<void>(inspectionError);
    return false;
#endif
}

std::filesystem::path resourceUserDataRoot(const std::filesystem::path& root) {
    if (root.empty()) {
        return {};
    }
    if (!root.is_absolute()) {
        throw std::invalid_argument("User data root must be absolute");
    }
    std::error_code error;
    const std::filesystem::file_status status =
        std::filesystem::symlink_status(root, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("Failed to inspect user data root: " +
                                 error.message());
    }
    if (std::filesystem::exists(status) &&
        (!std::filesystem::is_directory(status) ||
         isLinkLike(root, status, "Failed to inspect user data root"))) {
        throw std::invalid_argument("User data root must be a real directory");
    }
    error.clear();
    const std::filesystem::path normalized =
        std::filesystem::weakly_canonical(root, error);
    if (error || normalized.empty()) {
        throw std::invalid_argument("Invalid user data root");
    }
    return normalized;
}

std::optional<std::filesystem::directory_entry> findLooseResource(
    const std::filesystem::path& root, const std::filesystem::path& relative) {
    if (root.empty()) {
        return std::nullopt;
    }
    if (relative.empty() || relative.is_absolute()) {
        throw std::invalid_argument("Resource path must be relative");
    }
    std::filesystem::path current = root;
    std::optional<std::filesystem::directory_entry> matched;
    for (const std::filesystem::path& segment : relative) {
        const std::string name = ludork::standard::pathToUtf8(segment);
        if (name.empty() || name == "." || name == ".." ||
            name.find('\\') != std::string::npos ||
            name.find(':') != std::string::npos ||
            name.find('\0') != std::string::npos) {
            throw std::invalid_argument("Resource path is not canonical");
        }
        std::error_code error;
        const std::filesystem::file_status currentStatus =
            std::filesystem::symlink_status(current, error);
        if (error == std::errc::no_such_file_or_directory ||
            (!error && !std::filesystem::exists(currentStatus))) {
            return std::nullopt;
        }
        if (error) {
            throw std::runtime_error("Failed to inspect resource directory: " +
                                     error.message());
        }
        if (isLinkLike(current, currentStatus,
                       "Failed to inspect resource directory")) {
            throw std::runtime_error("Resource symlinks are not supported: " +
                                     ludork::standard::pathToUtf8(current));
        }
        if (!std::filesystem::is_directory(currentStatus)) {
            throw std::runtime_error("Resource ancestor is not a directory: " +
                                     ludork::standard::pathToUtf8(current));
        }
        std::filesystem::directory_iterator iterator(current, error);
        if (error) {
            throw std::runtime_error(
                "Failed to enumerate resource directory: " + error.message());
        }
        matched.reset();
        const std::filesystem::directory_iterator end;
        while (iterator != end) {
            if (ludork::standard::pathToUtf8(iterator->path().filename()) ==
                name) {
                matched = *iterator;
                break;
            }
            iterator.increment(error);
            if (error) {
                throw std::runtime_error(
                    "Failed to enumerate resource directory: " +
                    error.message());
            }
        }
        if (!matched) {
            return std::nullopt;
        }
        const std::filesystem::file_status status =
            matched->symlink_status(error);
        if (error) {
            throw std::runtime_error("Failed to inspect resource: " +
                                     error.message());
        }
        if (isLinkLike(matched->path(), status, "Failed to inspect resource")) {
            throw std::runtime_error(
                "Resource symlinks are not supported: " +
                ludork::standard::pathToUtf8(matched->path()));
        }
        if (!std::filesystem::is_directory(status) &&
            !std::filesystem::is_regular_file(status)) {
            throw std::runtime_error(
                "Unsupported resource entry: " +
                ludork::standard::pathToUtf8(matched->path()));
        }
        if (name == ".DS_Store" && std::filesystem::is_regular_file(status)) {
            return std::nullopt;
        }
        current = matched->path();
    }
    return matched;
}

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
        normalized /
        (name + (packed ? ludork::generated::resources::PackageExtension : ""));
    const std::filesystem::path alternate =
        normalized /
        (name + (packed ? "" : ludork::generated::resources::PackageExtension));
    const std::filesystem::file_status alternateStatus =
        std::filesystem::symlink_status(alternate, error);
    if (error && error != std::errc::no_such_file_or_directory) {
        throw std::runtime_error("Failed to inspect " + name + ": " +
                                 error.message());
    }
    if (std::filesystem::exists(alternateStatus)) {
        throw std::runtime_error(
            "Runtime root must contain only " + name +
            (packed ? ludork::generated::resources::PackageExtension : "") +
            " for the selected resource layout");
    }
    error.clear();
    const std::filesystem::file_status status =
        std::filesystem::symlink_status(selected, error);
    if (error || (packed ? !std::filesystem::is_regular_file(status)
                         : !std::filesystem::is_directory(status))) {
        throw std::runtime_error(
            "Runtime root must contain " + name +
            (packed ? ludork::generated::resources::PackageExtension : ""));
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
