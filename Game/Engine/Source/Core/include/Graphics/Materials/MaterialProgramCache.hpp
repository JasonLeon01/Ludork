#pragma once

#include <EngineRuntimeApi.hpp>
#include <Graphics/Materials/MaterialCompiler.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Transform.hpp>

#include <memory>
#include <cstdint>

namespace ludork::engine::materials {

class LUDORK_ENGINE_API MaterialProgramCache {
public:
    struct Context {
        sf::Vector2u textureSize{1, 1};
        sf::IntRect textureRect{{0, 0}, {1, 1}};
        sf::Color tint = sf::Color::White;
        float time = 0.0f;
        bool tile = false;
        float cellSize = 32.0f;
        sf::Transform worldTransform;
        sf::Vector2u targetSize{1, 1};
        sf::FloatRect worldView{{0.0f, 0.0f}, {1.0f, 1.0f}};
        float viewRotation = 0.0f;
        const sf::Texture* ambientLight = nullptr;
        const sf::Texture* directLight = nullptr;
        const sf::Texture* reflectionSource = nullptr;
        const sf::Texture* surfaceMask = nullptr;
        bool lighting = false;
        bool reflection = false;
        bool premultipliedSource = false;
    };

    MaterialProgramCache();
    ~MaterialProgramCache();
    MaterialProgramCache(const MaterialProgramCache&) = delete;
    MaterialProgramCache& operator=(const MaterialProgramCache&) = delete;

    std::shared_ptr<sf::Shader> bind(const MaterialResource& material,
                                     const RuntimeData::Map& parameters,
                                     const MaterialResource* layer,
                                     const RuntimeData::Map& layerParameters,
                                     MaterialCompiler::Pass pass,
                                     const Context& context);
    void clear();
    std::uint64_t bindCount() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::engine::materials
