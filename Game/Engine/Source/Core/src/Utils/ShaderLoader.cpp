#include <Utils/ShaderLoader.hpp>
#include <StringUtils.hpp>
#include <LudorkGenerated/EncryptedPayloadConstants.hpp>
#include <LudorkGenerated/ResourceFileConstants.hpp>

#include <EncryptedPayload.hpp>
#include <Runtime/AssetPath.hpp>
#include <Runtime/AssetStore.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr ludork::standard::EncryptedPayloadFormat ShaderFormat{
    .magic = ludork::generated::encrypted::ShaderMagic,
    .maximumSourceSize = ludork::generated::encrypted::MaximumShaderSize,
    .formatName = "shader",
};

bool isEncryptedExtension(const std::string& extension) {
    const auto& extensions = ludork::generated::resources::ShaderExtensions;
    return std::any_of(extensions.begin(), extensions.end(),
                       [&extension](const auto& item) {
                           return extension == item.encrypted;
                       });
}

std::string pathExtension(const std::string& path) {
    const std::size_t separator = path.rfind('/');
    const std::size_t dot = path.rfind('.');
    return dot == std::string::npos ||
                   (separator != std::string::npos && dot < separator)
               ? std::string{}
               : ludork::standard::lowercase(path.substr(dot));
}

void validateRequestedShaderPath(const std::string& path) {
    static_cast<void>(ludork::runtime::AssetPath::parse(path));
    const std::string extension = pathExtension(path);
    if (isEncryptedExtension(extension)) {
        throw std::invalid_argument(
            "Shader asset path must use .frag, .vert, or .geom rather than "
            "an encrypted extension: " +
            path);
    }
    const auto& extensions = ludork::generated::resources::ShaderExtensions;
    if (std::none_of(extensions.begin(), extensions.end(),
                     [&extension](const auto& item) {
                         return extension == item.source;
                     })) {
        throw std::invalid_argument(
            "Shader asset path must use .frag, .vert, or .geom: " + path);
    }
}

std::optional<sf::Shader::Type> shaderTypeFromExtension(
    const std::string& path) {
    std::string extension = pathExtension(path);
    for (const auto& item : ludork::generated::resources::ShaderExtensions) {
        if (extension == item.encrypted) {
            extension = item.source;
            break;
        }
    }
    if (extension == ".vert") {
        return sf::Shader::Type::Vertex;
    }
    if (extension == ".geom") {
        return sf::Shader::Type::Geometry;
    }
    if (extension == ".frag") {
        return sf::Shader::Type::Fragment;
    }
    return std::nullopt;
}

std::vector<std::string> shaderCandidates(const std::string& value) {
    validateRequestedShaderPath(value);
    std::vector<std::string> result{value};
    const std::string extension = pathExtension(value);
    for (const auto& item : ludork::generated::resources::ShaderExtensions) {
        if (extension == item.source) {
            result.push_back(
                value + std::string(item.encrypted.substr(item.source.size())));
            break;
        }
    }
    return result;
}

ShaderLoader::ShaderSourceResult decodeShader(
    const std::string& path, const std::vector<std::uint8_t>& encoded) {
    try {
        return {ludork::standard::decodeEncryptedPayload(encoded, ShaderFormat,
                                                         path),
                path,
                {}};
    } catch (const std::runtime_error& error) {
        return {{}, path, error.what()};
    }
}

ShaderLoader::ShaderSourceResult readShaderSource(const std::string& value) {
    const auto candidates = shaderCandidates(value);
    try {
        const auto source = ludork::runtime::assetStore().readFirst(candidates);
        if (!source) {
            return {{}, value, "Shader file not found: " + value};
        }
        if (isEncryptedExtension(pathExtension(source->path))) {
            return decodeShader(source->path, source->bytes);
        }
        return {std::string(source->bytes.begin(), source->bytes.end()),
                source->path,
                {}};
    } catch (const std::exception& error) {
        return {{},
                value,
                "Shader file could not be read: " + value + " (" +
                    error.what() + ")"};
    }
}

ShaderLoadResult failedLoad(const ShaderLoader::ShaderSourceResult& source) {
    return {nullptr, {}, source.resolvedPath, source.error};
}

}  // namespace

ShaderLoader::ShaderSourceResult ShaderLoader::readSource(
    const std::string& shaderPath) {
    return readShaderSource(shaderPath);
}

std::optional<sf::Shader::Type> ShaderLoader::inferType(
    const std::string& shaderPath) {
    validateRequestedShaderPath(shaderPath);
    return shaderTypeFromExtension(shaderPath);
}

ShaderLoadResult ShaderLoader::load(
    const std::string& shaderPath, std::optional<sf::Shader::Type> shaderType) {
    ShaderLoader::ShaderSourceResult source = readSource(shaderPath);
    if (!source) {
        return failedLoad(source);
    }
    const sf::Shader::Type type =
        shaderType.value_or(shaderTypeFromExtension(source.resolvedPath)
                                .value_or(sf::Shader::Type::Fragment));
    std::shared_ptr<sf::Shader> shader = std::make_shared<sf::Shader>();
    if (!shader->loadFromMemory(source.source, type)) {
        return {nullptr, std::move(source.source), source.resolvedPath,
                "Shader load failed: " + source.resolvedPath};
    }
    return {
        std::move(shader), std::move(source.source), source.resolvedPath, {}};
}

ShaderLoadResult ShaderLoader::load(const std::string& vertexPath,
                                    const std::string& fragmentPath) {
    ShaderLoader::ShaderSourceResult vertex = readSource(vertexPath);
    if (!vertex) {
        return failedLoad(vertex);
    }
    ShaderLoader::ShaderSourceResult fragment = readSource(fragmentPath);
    if (!fragment) {
        return failedLoad(fragment);
    }
    std::shared_ptr<sf::Shader> shader = std::make_shared<sf::Shader>();
    if (!shader->loadFromMemory(vertex.source, fragment.source)) {
        return {nullptr,
                {},
                vertex.resolvedPath,
                "Full shader load failed: " + vertex.resolvedPath + " and " +
                    fragment.resolvedPath};
    }
    return {std::move(shader), {}, vertex.resolvedPath, {}};
}

ShaderLoadResult ShaderLoader::load(const std::string& vertexPath,
                                    const std::string& geometryPath,
                                    const std::string& fragmentPath) {
    ShaderLoader::ShaderSourceResult vertex = readSource(vertexPath);
    if (!vertex) {
        return failedLoad(vertex);
    }
    ShaderLoader::ShaderSourceResult geometry = readSource(geometryPath);
    if (!geometry) {
        return failedLoad(geometry);
    }
    ShaderLoader::ShaderSourceResult fragment = readSource(fragmentPath);
    if (!fragment) {
        return failedLoad(fragment);
    }
    std::shared_ptr<sf::Shader> shader = std::make_shared<sf::Shader>();
    if (!shader->loadFromMemory(vertex.source, geometry.source,
                                fragment.source)) {
        return {nullptr,
                {},
                vertex.resolvedPath,
                "Shader load with geometry failed: " + vertex.resolvedPath +
                    ", " + geometry.resolvedPath + ", and " +
                    fragment.resolvedPath};
    }
    return {std::move(shader), {}, vertex.resolvedPath, {}};
}
