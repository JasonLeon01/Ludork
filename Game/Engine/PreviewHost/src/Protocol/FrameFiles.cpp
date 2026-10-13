#include "Protocol/FrameFiles.hpp"

#include "Protocol/FrameFilesImpl.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <system_error>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace ludork::preview_host {

FrameFiles::Impl::Impl() : temporary(std::filesystem::temp_directory_path()) {
    static std::atomic<std::uint64_t> sequence = 0;
    const auto stamp =
        std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
    const auto process = GetCurrentProcessId();
#else
    const auto process = getpid();
#endif
    prefix = "LudorkPreview-" + std::to_string(process) + "-" +
             std::to_string(stamp) + "-" + std::to_string(sequence++);
}

FrameFiles::Impl::Slot::~Slot() {
    reset();
}

void FrameFiles::Impl::Slot::reset() {
#ifdef _WIN32
    if (data != nullptr) {
        UnmapViewOfFile(data);
    }
    if (mapping != nullptr) {
        CloseHandle(mapping);
    }
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    mapping = nullptr;
    file = INVALID_HANDLE_VALUE;
#else
    if (data != nullptr) {
        munmap(data, capacity);
    }
    if (file != -1) {
        close(file);
    }
    file = -1;
#endif
    data = nullptr;
    capacity = 0;
    if (!path.empty()) {
        std::error_code error;
        std::filesystem::remove(path, error);
        path.clear();
    }
}

void FrameFiles::Impl::Slot::allocate(const std::filesystem::path& nextPath,
                                      std::size_t byteCount) {
    reset();
    path = nextPath;
#ifdef _WIN32
    file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       nullptr, CREATE_NEW,
                       FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                       nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        throw std::system_error(GetLastError(), std::system_category(),
                                "Create preview frame buffer");
    }
    LARGE_INTEGER size;
    size.QuadPart = static_cast<LONGLONG>(byteCount);
    if (!SetFilePointerEx(file, size, nullptr, FILE_BEGIN) ||
        !SetEndOfFile(file)) {
        throw std::system_error(GetLastError(), std::system_category(),
                                "Size preview frame buffer");
    }
    mapping = CreateFileMappingW(file, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (mapping == nullptr) {
        throw std::system_error(GetLastError(), std::system_category(),
                                "Map preview frame buffer");
    }
    data = static_cast<std::uint8_t*>(
        MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, byteCount));
    if (data == nullptr) {
        throw std::system_error(GetLastError(), std::system_category(),
                                "Open preview frame buffer view");
    }
#else
    file = open(path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (file == -1) {
        throw std::system_error(errno, std::generic_category(),
                                "Create preview frame buffer");
    }
    if (ftruncate(file, static_cast<off_t>(byteCount)) != 0) {
        throw std::system_error(errno, std::generic_category(),
                                "Size preview frame buffer");
    }
    void* view =
        mmap(nullptr, byteCount, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    if (view == MAP_FAILED) {
        throw std::system_error(errno, std::generic_category(),
                                "Map preview frame buffer");
    }
    data = static_cast<std::uint8_t*>(view);
#endif
    capacity = byteCount;
}

FrameFiles::FrameFiles() : impl_(std::make_unique<Impl>()) {}
FrameFiles::~FrameFiles() = default;

std::span<std::uint8_t> FrameFiles::acquire(std::size_t byteCount) {
    if (byteCount >
        static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::overflow_error("Preview frame buffer is too large");
    }
    impl_->current = (impl_->current + 1) % impl_->slots.size();
    Impl::Slot& slot = impl_->slots[impl_->current];
    const std::size_t capacity = std::max<std::size_t>(1, byteCount);
    if (slot.capacity < capacity) {
        const std::filesystem::path nextPath =
            impl_->temporary /
            (impl_->prefix + "-" + std::to_string(impl_->current) + "-" +
             std::to_string(++slot.revision) + ".bin");
        slot.allocate(nextPath, capacity);
    }
    return {slot.data, byteCount};
}

const std::filesystem::path& FrameFiles::path() const {
    return impl_->slots[impl_->current].path;
}

const std::filesystem::path& FrameFiles::write(
    const std::vector<std::uint8_t>& pixels) {
    std::span<std::uint8_t> target = acquire(pixels.size());
    if (!pixels.empty()) {
        std::memcpy(target.data(), pixels.data(), pixels.size());
    }
    return path();
}

}  // namespace ludork::preview_host
