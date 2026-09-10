#pragma once

#include <RuntimeApi.hpp>
#include <SFML/System/InputStream.hpp>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace ludork::runtime {

class LUDORK_RUNTIME_API AssetInputStream final : public sf::InputStream {
public:
    ~AssetInputStream() override;

    AssetInputStream(const AssetInputStream&) = delete;
    AssetInputStream& operator=(const AssetInputStream&) = delete;
    AssetInputStream(AssetInputStream&&) noexcept;
    AssetInputStream& operator=(AssetInputStream&&) noexcept;

    [[nodiscard]] std::optional<std::size_t> read(void* data,
                                                  std::size_t size) override;
    [[nodiscard]] std::optional<std::size_t> seek(
        std::size_t position) override;
    [[nodiscard]] std::optional<std::size_t> tell() override;
    [[nodiscard]] std::optional<std::size_t> getSize() override;

private:
    friend class AssetStore;
    struct Impl;

    AssetInputStream(const std::filesystem::path& source, std::uint64_t offset,
                     std::uint64_t size);

    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::runtime
