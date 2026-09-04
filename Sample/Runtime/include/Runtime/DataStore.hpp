#pragma once

#include <RuntimeApi.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace ludork::runtime {

enum class DataStoreMode : std::uint8_t {
    Loose,
    Packed,
};

struct LUDORK_RUNTIME_API DataStat {
    bool directory = false;
    std::uint64_t size = 0;
    double modificationTime = 0.0;
};

class LUDORK_RUNTIME_API DataStore final {
public:
    DataStore();
    ~DataStore();

    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;

    void configure(const std::filesystem::path& runtimeRoot,
                   DataStoreMode mode = DataStoreMode::Loose);
    void reset() noexcept;

    [[nodiscard]] bool isConfigured() const noexcept;
    [[nodiscard]] DataStoreMode mode() const;
    [[nodiscard]] bool handles(const std::filesystem::path& path) const;
    [[nodiscard]] std::optional<DataStat> stat(
        const std::filesystem::path& path) const;
    [[nodiscard]] std::vector<std::filesystem::path> listDirectory(
        const std::filesystem::path& path) const;
    [[nodiscard]] std::vector<std::uint8_t> readAll(
        const std::filesystem::path& path) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] LUDORK_RUNTIME_API DataStore& dataStore();

}  // namespace ludork::runtime
