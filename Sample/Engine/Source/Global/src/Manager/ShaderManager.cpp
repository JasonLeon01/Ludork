#include "ShaderManagerImpl.hpp"
#include <Manager/ShaderManager.hpp>

#include <Runtime/ConcurrentResourceCache.hpp>
#include <Utils/ShaderLoader.hpp>

#include <stdexcept>

namespace {

ShaderManager::Impl& shaderManagerImpl() {
    static ShaderManager::Impl impl;
    return impl;
}

}  // namespace

std::shared_ptr<sf::Shader> ShaderManager::load(
    const std::string& shaderPath, std::optional<sf::Shader::Type> shaderType) {
    const sf::Shader::Type type =
        shaderType.value_or(ShaderLoader::inferType(shaderPath)
                                .value_or(sf::Shader::Type::Fragment));
    const std::string key =
        shaderPath + '\0' + std::to_string(static_cast<int>(type));
    return shaderManagerImpl().shaders.getOrLoad(key, [&]() {
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
    return shaderManagerImpl().fullShaders.getOrLoad(key, [&]() {
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
    return shaderManagerImpl().geoShaders.getOrLoad(key, [&]() {
        ShaderLoadResult result =
            ShaderLoader::load(vertPath, geoPath, fragPath);
        if (!result) {
            throw std::runtime_error(result.error);
        }
        return result.shader;
    });
}

std::size_t ShaderManager::getMemory() {
    ShaderManager::Impl& impl = shaderManagerImpl();
    const std::size_t entries = impl.shaders.entryCount() +
                                impl.fullShaders.entryCount() +
                                impl.geoShaders.entryCount();
    return sizeof(impl) +
           entries * (sizeof(sf::Shader) + sizeof(std::weak_ptr<sf::Shader>));
}

void ShaderManager::clear() noexcept {
    ShaderManager::Impl& impl = shaderManagerImpl();
    impl.geoShaders.clear();
    impl.fullShaders.clear();
    impl.shaders.clear();
}
