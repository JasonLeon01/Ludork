#pragma once

#include <BindAnnotations.hpp>
#include <GlobalRuntimeApi.hpp>

#include <SFML/Graphics/Shader.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

BIND_CLASS()
class LUDORK_GLOBAL_API ShaderManager {
public:
    BIND_METHOD(defaults = {nil})
    static std::shared_ptr<sf::Shader> load(
        const std::string& shaderPath,
        std::optional<sf::Shader::Type> shaderType = std::nullopt);

    BIND_METHOD()
    static std::shared_ptr<sf::Shader> loadFull(const std::string& vertPath,
                                                const std::string& fragPath);

    BIND_METHOD()
    static std::shared_ptr<sf::Shader> loadFullShaderWithGeo(
        const std::string& vertPath, const std::string& geoPath,
        const std::string& fragPath);

    BIND_METHOD()
    static std::size_t getMemory();

    BIND_IGNORE()
    static void clear() noexcept;

};
