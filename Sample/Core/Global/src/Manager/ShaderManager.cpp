#include <Manager/ShaderManager.hpp>

#include <ConcurrentResourceCache.hpp>
#include <Utils/ShaderLoader.hpp>

#include <stdexcept>

namespace {

using ShaderCache = ludork::core::ConcurrentResourceCache<sf::Shader>;

struct ShaderCaches {
    ShaderCache shaders;
    ShaderCache fullShaders;
    ShaderCache geoShaders;
};

ShaderCaches& shaderCaches() {
    static ShaderCaches caches;
    return caches;
}

}  // namespace

std::shared_ptr<sf::Shader> ShaderManager::load(
    const std::string& shaderPath, std::optional<sf::Shader::Type> shaderType) {
    const sf::Shader::Type type =
        shaderType.value_or(ShaderLoader::inferType(shaderPath)
                                .value_or(sf::Shader::Type::Fragment));
    const std::string key =
        shaderPath + '\0' + std::to_string(static_cast<int>(type));
    return shaderCaches().shaders.getOrLoad(key, [&]() {
        ShaderLoadResult result = ShaderLoader::load(shaderPath, shaderType);
        if (!result) {
            throw std::runtime_error(result.error);
        }
        return result.shader;
    });
}

std::shared_ptr<sf::Shader> ShaderManager::loadFull(
    const std::string& vertPath, const std::string& fragPath) {
    const std::string key = vertPath + '\0' + fragPath;
    return shaderCaches().fullShaders.getOrLoad(key, [&]() {
        ShaderLoadResult result = ShaderLoader::load(vertPath, fragPath);
        if (!result) {
            throw std::runtime_error(result.error);
        }
        return result.shader;
    });
}

std::shared_ptr<sf::Shader> ShaderManager::loadFullShaderWithGeo(
    const std::string& vertPath, const std::string& geoPath,
    const std::string& fragPath) {
    const std::string key = vertPath + '\0' + geoPath + '\0' + fragPath;
    return shaderCaches().geoShaders.getOrLoad(key, [&]() {
        ShaderLoadResult result =
            ShaderLoader::load(vertPath, geoPath, fragPath);
        if (!result) {
            throw std::runtime_error(result.error);
        }
        return result.shader;
    });
}

std::size_t ShaderManager::getMemory() {
    ShaderCaches& caches = shaderCaches();
    const std::size_t entries = caches.shaders.entryCount() +
                                caches.fullShaders.entryCount() +
                                caches.geoShaders.entryCount();
    return sizeof(caches) +
           entries * (sizeof(sf::Shader) + sizeof(std::weak_ptr<sf::Shader>));
}

void ShaderManager::clear() noexcept {
    ShaderCaches& caches = shaderCaches();
    caches.geoShaders.clear();
    caches.fullShaders.clear();
    caches.shaders.clear();
}
