#pragma once

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ludork::global::texture_manager_impl {

enum class LoadStage {
    Queued,
    Decoding,
    Ready,
    Uploading,
    Completed,
    Failed,
    Cancelled
};

struct DecodeTicket {
    std::string key;
    std::string filePath;
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    int priority = 0;
    LoadStage stage = LoadStage::Queued;
    std::unique_ptr<sf::Image> image;
    std::exception_ptr failure;
    std::size_t bytes = 0;
};

struct TextureEntry;

struct RequestState {
    std::shared_ptr<TextureEntry> entry;
    std::shared_ptr<sf::Texture> texture;
    int priority = 0;
    bool cancelled = false;
};

struct TextureEntry {
    std::shared_ptr<DecodeTicket> ticket;
    bool sRGB = false;
    std::optional<sf::IntRect> area;
    bool smooth = false;
    std::weak_ptr<sf::Texture> texture;
    std::shared_ptr<sf::Texture> pinnedTexture;
    std::vector<std::weak_ptr<RequestState>> requests;
};

}  // namespace ludork::global::texture_manager_impl
