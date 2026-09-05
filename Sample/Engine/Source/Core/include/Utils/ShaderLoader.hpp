#pragma once

#include <EngineRuntimeApi.hpp>

#include <SFML/Graphics/Shader.hpp>

#include <memory>
#include <optional>
#include <string>

struct LUDORK_ENGINE_API ShaderSourceResult {
    std::string source;
    std::string resolvedPath;
    std::string error;

    explicit operator bool() const noexcept {
        return error.empty();
    }
};

struct LUDORK_ENGINE_API ShaderLoadResult {
    std::shared_ptr<sf::Shader> shader;
    std::string source;
    std::string resolvedPath;
    std::string error;

    explicit operator bool() const noexcept {
        return shader != nullptr;
    }
};

class LUDORK_ENGINE_API ShaderLoader {
public:
    static ShaderSourceResult readSource(const std::string& shaderPath);

    static std::optional<sf::Shader::Type> inferType(
        const std::string& shaderPath);

    static ShaderLoadResult load(
        const std::string& shaderPath,
        std::optional<sf::Shader::Type> shaderType = std::nullopt);

    static ShaderLoadResult load(const std::string& vertexPath,
                                 const std::string& fragmentPath);

    static ShaderLoadResult load(const std::string& vertexPath,
                                 const std::string& geometryPath,
                                 const std::string& fragmentPath);
};
