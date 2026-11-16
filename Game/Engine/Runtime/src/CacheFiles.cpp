#include <Runtime/CacheFiles.hpp>
#include <Utf8Path.hpp>

#include <fstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <algorithm>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ludork::runtime {
namespace {

template <typename Operation>
std::error_code retryFileOperation(const std::atomic<bool>& stop,
                                   const Operation& operation) {
#if defined(_WIN32)
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);
    auto delay = std::chrono::milliseconds(50);
#endif
    for (;;) {
        if (stop.load()) {
            throw std::runtime_error("Cache preparation was cancelled");
        }
        std::error_code error;
        operation(error);
#if defined(_WIN32)
        const bool transient = error.category() == std::system_category() &&
                               (error.value() == ERROR_ACCESS_DENIED ||
                                error.value() == ERROR_SHARING_VIOLATION ||
                                error.value() == ERROR_LOCK_VIOLATION);
        if (transient && std::chrono::steady_clock::now() < deadline) {
            const auto wake =
                std::min(deadline, std::chrono::steady_clock::now() + delay);
            while (std::chrono::steady_clock::now() < wake) {
                if (stop.load()) {
                    throw std::runtime_error("Cache preparation was cancelled");
                }
                std::this_thread::sleep_until(
                    std::min(wake, std::chrono::steady_clock::now() +
                                       std::chrono::milliseconds(10)));
            }
            delay = std::min(delay * 2, std::chrono::milliseconds(400));
            continue;
        }
#endif
        return error;
    }
}

}  // namespace

void CacheFiles::rename(const std::filesystem::path& source,
                        const std::filesystem::path& destination,
                        const std::atomic<bool>& stop) {
    const auto error = retryFileOperation(stop, [&](std::error_code& result) {
        std::filesystem::rename(source, destination, result);
    });
    if (error) {
        throw std::filesystem::filesystem_error("Cannot rename cache", source,
                                                destination, error);
    }
}

void CacheFiles::remove(const std::filesystem::path& path,
                        const std::atomic<bool>& stop) {
    const auto error = retryFileOperation(stop, [&](std::error_code& result) {
        std::filesystem::remove(path, result);
    });
    if (error) {
        throw std::filesystem::filesystem_error("Cannot remove cache", path,
                                                error);
    }
}

std::string CacheFiles::read(const std::filesystem::path& path) {
    requireOrdinary(path);
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Cannot read cache: " +
                                 ludork::standard::pathToUtf8(path));
    }
    std::string bytes{std::istreambuf_iterator<char>(stream),
                      std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        throw std::runtime_error("Cannot read complete cache: " +
                                 ludork::standard::pathToUtf8(path));
    }
    return bytes;
}

bool CacheFiles::requireOrdinary(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory ||
        status.type() == std::filesystem::file_type::not_found) {
        return false;
    }
    if (error) {
        throw std::filesystem::filesystem_error("Cannot inspect cache", path,
                                                error);
    }
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        throw std::runtime_error("Cache links are not supported: " +
                                 ludork::standard::pathToUtf8(path));
    }
#endif
    if (std::filesystem::is_symlink(status) ||
        (!std::filesystem::is_directory(status) &&
         !std::filesystem::is_regular_file(status))) {
        throw std::runtime_error("Unsupported cache entry: " +
                                 ludork::standard::pathToUtf8(path));
    }
    return std::filesystem::is_directory(status);
}

void CacheFiles::requireTree(const std::filesystem::path& path) {
    if (requireOrdinary(path)) {
        for (const auto& child : std::filesystem::directory_iterator(path)) {
            requireTree(child.path());
        }
    }
}

}  // namespace ludork::runtime
