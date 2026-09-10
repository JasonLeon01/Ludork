#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/Image.hpp>

BIND_CLASS()
class FunctionalImage : public Image, public FunctionalBase {
public:
    BIND_INIT()
    explicit FunctionalImage(std::shared_ptr<sf::Texture> texture,
                             std::optional<sf::IntRect> rect = std::nullopt);
    virtual ~FunctionalImage() = default;
};
