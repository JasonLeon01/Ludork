#pragma once

#include <RuntimeApi.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ludork::runtime {

enum class AssetStoreMode : std::uint8_t {
    Loose,
    Packed
};

class AssetInputStream;

class LUDORK_RUNTIME_API AssetStore final {
public:
    struct LUDORK_RUNTIME_API AssetStat {
        bool directory = false;
        std::uint64_t size = 0;
        double modificationTime = 0.0;
    };

    AssetStore();
    ~AssetStore();

    AssetStore(const AssetStore&) = delete;
    AssetStore& operator=(const AssetStore&) = delete;

    void configure(const std::filesystem::path& runtimeRoot,
                   AssetStoreMode mode = AssetStoreMode::Loose);
    void reset() noexcept;

    [[nodiscard]] bool isConfigured() const noexcept;
    [[nodiscard]] AssetStoreMode mode() const;
    [[nodiscard]] bool exists(const std::string& assetPath) const;
    [[nodiscard]] std::optional<AssetStat> stat(
        const std::string& assetPath) const;
    [[nodiscard]] std::unique_ptr<AssetInputStream> open(
        const std::string& assetPath) const;
    [[nodiscard]] std::vector<std::uint8_t> readAll(
        const std::string& assetPath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] LUDORK_RUNTIME_API AssetStore& assetStore();

}  // namespace ludork::runtime
