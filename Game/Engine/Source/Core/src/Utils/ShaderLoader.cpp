#include <Utils/ShaderLoader.hpp>
#include <Runtime/Graphics/ShaderCache.hpp>
#include <Runtime/Graphics/SceneLighting.hpp>
#include <SFML/Graphics/GraphicsBackend.hpp>
#include <StringUtils.hpp>
#include <Sha256.hpp>
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
#include <utility>

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
                {},
                ludork::standard::sha256(std::string_view(
                    reinterpret_cast<const char*>(encoded.data()),
                    encoded.size()))};
    } catch (const std::runtime_error& error) {
        return {{}, path, error.what()};
    }
}

ShaderLoader::ShaderSourceResult readShaderSource(
    const std::string& value,
    const std::map<std::string, std::string>* projectedSources) {
    const auto candidates = shaderCandidates(value);
    try {
        std::optional<ludork::runtime::AssetStore::AssetContents> source;
        if (projectedSources &&
            value.starts_with(ludork::runtime::graphics::ShaderCache::
                                  MaterialProjection::Prefix)) {
            for (const auto& candidate : candidates) {
                if (auto found = projectedSources->find(candidate);
                    found != projectedSources->end()) {
                    source.emplace(ludork::runtime::AssetStore::AssetContents{
                        candidate,
                        {found->second.begin(), found->second.end()}});
                    break;
                }
            }
        } else {
            source = ludork::runtime::assetStore().readFirst(candidates);
        }
        if (!source) {
            return {{}, value, "Shader file not found: " + value};
        }
        if (isEncryptedExtension(pathExtension(source->path))) {
            return decodeShader(source->path, source->bytes);
        }
        return {std::string(source->bytes.begin(), source->bytes.end()),
                source->path,
                {},
                ludork::standard::sha256(std::string_view(
                    reinterpret_cast<const char*>(source->bytes.data()),
                    source->bytes.size()))};
    } catch (const std::exception& error) {
        return {{},
                value,
                "Shader file could not be read: " + value + " (" +
                    error.what() + ")"};
    }
}

ShaderLoadResult loadProgram(
    const std::vector<std::pair<std::string, sf::Shader::Type>>& stages) {
    ludork::runtime::graphics::ShaderCache::Program program;
    try {
        const auto device = sf::rhi::getDevice();
        const bool preview =
            device && device->backend() == sf::GraphicsBackend::OpenGL;
        for (const auto& [path, type] : stages) {
            validateRequestedShaderPath(path);
            const auto stage =
                type == sf::Shader::Type::Vertex
                    ? ludork::runtime::graphics::ShaderTranslator::Stage::Vertex
                : type == sf::Shader::Type::Geometry
                    ? ludork::runtime::graphics::ShaderTranslator::Stage::
                          Geometry
                    : ludork::runtime::graphics::ShaderTranslator::Stage::
                          Fragment;
            std::string text;
            if (preview) {
                auto source = ShaderLoader::readSource(path);
                if (!source) {
                    return {nullptr, {}, source.resolvedPath, source.error};
                }
                text = std::move(source.source);
            }
            program.source.stages.push_back({stage, std::move(text), path});
        }
        return {ludork::runtime::graphics::ShaderCache::instantiate(program),
                {},
                stages.front().first,
                {}};
    } catch (const std::exception& error) {
        return {nullptr, {}, stages.front().first, error.what()};
    }
}

}  // namespace

ShaderLoader::ShaderSourceResult ShaderLoader::readSource(
    const std::string& shaderPath,
    const std::map<std::string, std::string>* projectedSources) {
    auto result = readShaderSource(shaderPath, projectedSources);
    if (result) {
        const std::string marker = "// LUDORK_SCENE_LIGHTING";
        const auto position = result.source.find(marker);
        if (position != std::string::npos) {
            result.source.replace(
                position, marker.size(),
                ludork::runtime::graphics::sceneLightingGlsl());
        }
    }
    return result;
}

std::optional<sf::Shader::Type> ShaderLoader::inferType(
    const std::string& shaderPath) {
    validateRequestedShaderPath(shaderPath);
    return shaderTypeFromExtension(shaderPath);
}

ShaderLoadResult ShaderLoader::load(
    const std::string& shaderPath, std::optional<sf::Shader::Type> shaderType) {
    return loadProgram(
        {{shaderPath, shaderType.value_or(inferType(shaderPath).value())}});
}

ShaderLoadResult ShaderLoader::load(const std::string& vertexPath,
                                    const std::string& fragmentPath) {
    return loadProgram({{vertexPath, sf::Shader::Type::Vertex},
                        {fragmentPath, sf::Shader::Type::Fragment}});
}

ShaderLoadResult ShaderLoader::load(const std::string& vertexPath,
                                    const std::string& geometryPath,
                                    const std::string& fragmentPath) {
    return loadProgram({{vertexPath, sf::Shader::Type::Vertex},
                        {geometryPath, sf::Shader::Type::Geometry},
                        {fragmentPath, sf::Shader::Type::Fragment}});
}
