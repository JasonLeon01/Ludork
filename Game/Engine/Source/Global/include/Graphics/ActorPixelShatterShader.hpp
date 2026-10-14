#pragma once

#include <CoreMinimal.hpp>
#include <GlobalRuntimeApi.hpp>

BIND_CLASS()
class LUDORK_GLOBAL_API ActorPixelShatterShader {
public:
    BIND_METHOD()
    static std::shared_ptr<sf::Shader> load();

    BIND_METHOD()
    static void bindLighting(const std::shared_ptr<sf::Shader>& shader);
};
