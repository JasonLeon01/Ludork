#pragma once

#include <RuntimeApi.hpp>

#include <atomic>
#include <filesystem>
#include <string>

namespace ludork::runtime {

class LUDORK_RUNTIME_API CacheFiles {
public:
    static std::string read(const std::filesystem::path& path);
    static bool requireOrdinary(const std::filesystem::path& path);
    static void requireTree(const std::filesystem::path& path);
    static void rename(const std::filesystem::path& source,
                       const std::filesystem::path& destination,
                       const std::atomic<bool>& stop);
    static void remove(const std::filesystem::path& path,
                       const std::atomic<bool>& stop);
};

}  // namespace ludork::runtime
