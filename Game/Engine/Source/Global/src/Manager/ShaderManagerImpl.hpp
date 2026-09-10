#pragma once
#include <Manager/ShaderManager.hpp>
#include <Runtime/ConcurrentResourceCache.hpp>

struct ShaderManager::Impl {
    ludork::runtime::ConcurrentResourceCache<sf::Shader> shaders;
    ludork::runtime::ConcurrentResourceCache<sf::Shader> fullShaders;
    ludork::runtime::ConcurrentResourceCache<sf::Shader> geoShaders;
};
