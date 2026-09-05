#pragma once

#include <memory>
#include <string>

namespace sf {
class Shader;
}

namespace ludork::engine::text_effects {

std::shared_ptr<sf::Shader> loadShader();
void warnOnce(const std::string& key, const std::string& message);
void clearResources() noexcept;

}  // namespace ludork::engine::text_effects
