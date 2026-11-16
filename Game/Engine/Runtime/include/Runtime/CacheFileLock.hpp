#pragma once

#include <RuntimeApi.hpp>

#include <atomic>
#include <cstdint>
#include <filesystem>

namespace ludork::runtime {

class LUDORK_RUNTIME_API CacheFileLock {
public:
    CacheFileLock(const std::filesystem::path& path,
                  const std::atomic<bool>& stop);
    ~CacheFileLock();
    CacheFileLock(const CacheFileLock&) = delete;
    CacheFileLock& operator=(const CacheFileLock&) = delete;

private:
    std::intptr_t handle_ = -1;
#ifdef __EMSCRIPTEN__
    std::filesystem::path webKey_;
#endif
};

}  // namespace ludork::runtime
