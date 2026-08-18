#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <vector>

namespace sf {

class RenderTarget;
class RenderTexture;
class Shader;
class Sprite;
class Text;
class Texture;
struct RenderStates;

}  // namespace sf

struct TextGlowConfig;
struct TextGradientConfig;

namespace ludork::engine::text_effects {

struct Source {
    const sf::Text* text = nullptr;
    sf::Color fillColor = sf::Color::White;
    sf::Color outlineColor = sf::Color::Black;
    sf::Transform transform;
};

struct Cache {
    Cache();
    ~Cache();

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    bool dirty = true;
    sf::FloatRect pixelBounds;
    sf::Vector2f contentMinimum;
    sf::Vector2f contentMaximum;
    std::unique_ptr<sf::RenderTexture> fill;
    std::unique_ptr<sf::RenderTexture> outline;
    std::unique_ptr<sf::Texture> curve;
    std::unique_ptr<sf::Sprite> sprite;
    std::shared_ptr<sf::Shader> shader;
};

bool enabled(const TextGlowConfig& glow,
             const TextGradientConfig& gradient);
sf::FloatRect expandedBounds(const sf::FloatRect& bounds,
                             const TextGlowConfig& glow);
void rebuild(Cache& cache, const std::vector<Source>& sources,
             const TextGlowConfig& glow,
             const TextGradientConfig& gradient);
bool draw(Cache& cache, sf::RenderTarget& target, sf::RenderStates states,
          const sf::Color& colour, const TextGlowConfig& glow,
          const TextGradientConfig& gradient);

}  // namespace ludork::engine::text_effects
