#include <Manager/ShaderManager.hpp>

#include <Utils/ShaderLoader.hpp>

#include <stdexcept>

std::unordered_map<std::string, std::weak_ptr<sf::Shader>>
    ShaderManager::shaders_;
std::unordered_map<std::string, std::weak_ptr<sf::Shader>>
    ShaderManager::fullShaders_;
std::unordered_map<std::string, std::weak_ptr<sf::Shader>>
    ShaderManager::geoShaders_;

std::shared_ptr<sf::Shader> ShaderManager::load(
    const std::string& shaderPath, std::optional<sf::Shader::Type> shaderType) {
    const sf::Shader::Type type =
        shaderType.value_or(ShaderLoader::inferType(shaderPath)
                                .value_or(sf::Shader::Type::Fragment));
    const std::string key =
        shaderPath + '\0' + std::to_string(static_cast<int>(type));
    const auto iterator = shaders_.find(key);
    if (iterator != shaders_.end()) {
        const std::shared_ptr<sf::Shader> cached = iterator->second.lock();
        if (cached != nullptr) {
            return cached;
        }
    }
    ShaderLoadResult result = ShaderLoader::load(shaderPath, shaderType);
    if (!result) {
        throw std::runtime_error(result.error);
    }
    shaders_[key] = result.shader;
    return result.shader;
}

std::shared_ptr<sf::Shader> ShaderManager::loadFull(
    const std::string& vertPath, const std::string& fragPath) {
    const std::string key = vertPath + '\0' + fragPath;
    const auto iterator = fullShaders_.find(key);
    if (iterator != fullShaders_.end()) {
        const std::shared_ptr<sf::Shader> cached = iterator->second.lock();
        if (cached != nullptr) {
            return cached;
        }
    }
    ShaderLoadResult result = ShaderLoader::load(vertPath, fragPath);
    if (!result) {
        throw std::runtime_error(result.error);
    }
    fullShaders_[key] = result.shader;
    return result.shader;
}

std::shared_ptr<sf::Shader> ShaderManager::loadFullShaderWithGeo(
    const std::string& vertPath, const std::string& geoPath,
    const std::string& fragPath) {
    const std::string key = vertPath + '\0' + geoPath + '\0' + fragPath;
    const auto iterator = geoShaders_.find(key);
    if (iterator != geoShaders_.end()) {
        const std::shared_ptr<sf::Shader> cached = iterator->second.lock();
        if (cached != nullptr) {
            return cached;
        }
    }
    ShaderLoadResult result = ShaderLoader::load(vertPath, geoPath, fragPath);
    if (!result) {
        throw std::runtime_error(result.error);
    }
    geoShaders_[key] = result.shader;
    return result.shader;
}

std::size_t ShaderManager::getMemory() {
    removeExpired();
    return sizeof(shaders_) + sizeof(fullShaders_) + sizeof(geoShaders_) +
           (shaders_.size() + fullShaders_.size() + geoShaders_.size()) *
               (sizeof(sf::Shader) + sizeof(std::weak_ptr<sf::Shader>));
}

void ShaderManager::clear() noexcept {
    geoShaders_.clear();
    fullShaders_.clear();
    shaders_.clear();
}

void ShaderManager::removeExpired() {
    const auto eraseExpired = [](auto& cache) {
        for (auto iterator = cache.begin(); iterator != cache.end();) {
            if (iterator->second.expired()) {
                iterator = cache.erase(iterator);
            } else {
                ++iterator;
            }
        }
    };
    eraseExpired(shaders_);
    eraseExpired(fullShaders_);
    eraseExpired(geoShaders_);
}
