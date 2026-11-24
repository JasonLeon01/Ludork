#pragma once

#include <memory>
#include <string>
#include <unordered_map>

namespace sf {
class Texture;
}

namespace ui_control_adapter_detail {

class TextureLoadScope {
public:
    using Cache = std::unordered_map<std::string, std::shared_ptr<sf::Texture>>;

    explicit TextureLoadScope(Cache& cache);
    ~TextureLoadScope();
    TextureLoadScope(const TextureLoadScope&) = delete;
    TextureLoadScope& operator=(const TextureLoadScope&) = delete;

    static std::shared_ptr<sf::Texture> load(const std::string& assetKey);

private:
    static thread_local Cache* current_;
    Cache* previous_;
};

}  // namespace ui_control_adapter_detail
