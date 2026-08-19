#pragma once

#include <SFML/Graphics.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using ShaderUniformValue =
    std::variant<float, int, bool, sf::Vector2f, sf::Vector3f, sf::Glsl::Vec4,
                 sf::Vector2i, sf::Vector3i, sf::Glsl::Ivec4, sf::Color,
                 std::shared_ptr<sf::Texture>, std::vector<float>,
                 std::vector<sf::Vector2f>, std::vector<sf::Vector3f>,
                 std::vector<sf::Glsl::Vec4>>;
using ShaderUniforms = std::unordered_map<std::string, ShaderUniformValue>;
