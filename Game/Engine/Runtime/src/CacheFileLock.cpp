#include <Runtime/CacheFileLock.hpp>
#include <Runtime/CacheFiles.hpp>

#include <cerrno>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace ludork::runtime {

CacheFileLock::CacheFileLock(const std::filesystem::path& path,
                             const std::atomic<bool>& stop) {
    CacheFiles::requireOrdinary(path);
    while (!stop.load()) {
#if defined(_WIN32)
        HANDLE file = CreateFileW(
            path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(file, &info) ||
                (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                CloseHandle(file);
                throw std::runtime_error("Invalid cache lock file");
            }
            handle_ = reinterpret_cast<std::intptr_t>(file);
            return;
        }
        const DWORD error = GetLastError();
        if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION) {
            throw std::system_error(static_cast<int>(error),
                                    std::system_category(),
                                    "Cannot lock cache");
        }
#else
        const int file =
            ::open(path.c_str(), O_CREAT | O_RDWR | O_NOFOLLOW, 0600);
        if (file == -1) {
            throw std::system_error(errno, std::generic_category(),
                                    "Cannot open cache lock");
        }
        if (::flock(file, LOCK_EX | LOCK_NB) == 0) {
            handle_ = file;
            return;
        }
        const int error = errno;
        ::close(file);
        if (error != EWOULDBLOCK && error != EAGAIN && error != EINTR) {
            throw std::system_error(error, std::generic_category(),
                                    "Cannot lock cache");
        }
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("Cache preparation was cancelled");
}

CacheFileLock::~CacheFileLock() {
    if (handle_ != -1) {
#if defined(_WIN32)
        CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
        ::flock(static_cast<int>(handle_), LOCK_UN);
        ::close(static_cast<int>(handle_));
#endif
    }
}

}  // namespace ludork::runtime
