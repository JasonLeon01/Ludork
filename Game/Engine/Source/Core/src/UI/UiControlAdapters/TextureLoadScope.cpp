#include "TextureLoadScope.hpp"

#include <Runtime/AssetInputStream.hpp>
#include <Runtime/AssetStore.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <stdexcept>
#include <utility>

namespace ui_control_adapter_detail {

thread_local TextureLoadScope::Cache* TextureLoadScope::current_ = nullptr;

TextureLoadScope::TextureLoadScope(Cache& cache)
    : previous_(std::exchange(current_, &cache)) {}

TextureLoadScope::~TextureLoadScope() {
    current_ = previous_;
}

std::shared_ptr<sf::Texture> TextureLoadScope::load(
    const std::string& assetKey) {
    if (current_ != nullptr) {
        const auto found = current_->find(assetKey);
        if (found != current_->end()) {
            return found->second;
        }
    }
    const auto stream = ludork::runtime::assetStore().open(assetKey);
    auto texture = std::make_shared<sf::Texture>();
    if (!texture->loadFromStream(*stream)) {
        throw std::runtime_error("Failed to load UI texture: " + assetKey);
    }
    if (current_ != nullptr) {
        current_->emplace(assetKey, texture);
    }
    return texture;
}

}  // namespace ui_control_adapter_detail
