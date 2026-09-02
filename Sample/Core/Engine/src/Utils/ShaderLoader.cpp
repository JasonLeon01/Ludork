#include <Utils/ShaderLoader.hpp>

#include <EncryptedPayload.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t MaximumShaderSize = 64U * 1024U * 1024U;
constexpr ludork::standard::EncryptedPayloadFormat ShaderFormat{
    .magic = {'L', 'D', 'S', 'C'},
    .maximumSourceSize = MaximumShaderSize,
    .formatName = "shader",
};

std::string lowerString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

std::string pathText(const std::filesystem::path& path) {
    return ludork::standard::pathToUtf8(path);
}

std::string normalizedShaderPath(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    while (value.starts_with("./")) {
        value.erase(0, 2);
    }
    return value;
}

bool isEncryptedExtension(const std::string& extension) {
    return extension == ".fragc" || extension == ".vertc" ||
           extension == ".geomc";
}

std::optional<sf::Shader::Type> shaderTypeFromExtension(
    const std::filesystem::path& path) {
    const std::string extension = lowerString(pathText(path.extension()));
    if (extension == ".vert" || extension == ".vertc") {
        return sf::Shader::Type::Vertex;
    }
    if (extension == ".geom" || extension == ".geomc") {
        return sf::Shader::Type::Geometry;
    }
    if (extension == ".frag" || extension == ".fragc") {
        return sf::Shader::Type::Fragment;
    }
    return std::nullopt;
}

std::filesystem::path shaderAssetPath(const std::string& value) {
    const std::string normalized = normalizedShaderPath(value);
    const std::filesystem::path path =
        ludork::standard::pathFromUtf8(normalized);
    if (path.is_absolute()) {
        return path;
    }
    const std::string lowerPath = lowerString(normalized);
    if (lowerPath.starts_with("assets/shaders/")) {
        return std::filesystem::current_path() / path;
    }
    return std::filesystem::current_path() / "Assets" / "Shaders" / path;
}

bool isRegularFile(const std::filesystem::path& path) {
    std::error_code error;
    const bool regular = std::filesystem::is_regular_file(path, error);
    return regular && !error;
}

std::filesystem::path resolveShaderPath(const std::string& value) {
    const std::filesystem::path requested = shaderAssetPath(value);
    if (isRegularFile(requested)) {
        return requested;
    }
    const std::string extension = lowerString(pathText(requested.extension()));
    if (extension == ".frag" || extension == ".vert" || extension == ".geom") {
        std::filesystem::path encrypted = requested;
        encrypted.replace_extension(extension + "c");
        if (isRegularFile(encrypted)) {
            return encrypted;
        }
    }
    return requested;
}

bool readFile(const std::filesystem::path& path,
              std::vector<std::uint8_t>& contents) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    contents.assign(std::istreambuf_iterator<char>(stream),
                    std::istreambuf_iterator<char>());
    return stream.good() || stream.eof();
}

ShaderSourceResult decodeShader(const std::filesystem::path& path,
                                const std::vector<std::uint8_t>& encoded) {
    try {
        return {ludork::standard::decodeEncryptedPayload(encoded, ShaderFormat,
                                                         pathText(path)),
                path,
                {}};
    } catch (const std::runtime_error& error) {
        return {{}, path, error.what()};
    }
}

ShaderSourceResult readShaderSource(const std::string& value) {
    const std::filesystem::path path = resolveShaderPath(value);
    if (!isRegularFile(path)) {
        return {{}, path, "Shader file not found: " + pathText(path)};
    }
    std::vector<std::uint8_t> contents;
    if (!readFile(path, contents)) {
        return {{}, path, "Shader file could not be read: " + pathText(path)};
    }
    const std::string extension = lowerString(pathText(path.extension()));
    if (isEncryptedExtension(extension)) {
        return decodeShader(path, contents);
    }
    return {std::string(contents.begin(), contents.end()), path, {}};
}

ShaderLoadResult failedLoad(const ShaderSourceResult& source) {
    return {nullptr, {}, source.resolvedPath, source.error};
}

}  // namespace

ShaderSourceResult ShaderLoader::readSource(const std::string& shaderPath) {
    return readShaderSource(shaderPath);
}

std::optional<sf::Shader::Type> ShaderLoader::inferType(
    const std::string& shaderPath) {
    return shaderTypeFromExtension(ludork::standard::pathFromUtf8(shaderPath));
}

ShaderLoadResult ShaderLoader::load(
    const std::string& shaderPath, std::optional<sf::Shader::Type> shaderType) {
    ShaderSourceResult source = readSource(shaderPath);
    if (!source) {
        return failedLoad(source);
    }
    const sf::Shader::Type type =
        shaderType.value_or(shaderTypeFromExtension(source.resolvedPath)
                                .value_or(sf::Shader::Type::Fragment));
    std::shared_ptr<sf::Shader> shader = std::make_shared<sf::Shader>();
    if (!shader->loadFromMemory(source.source, type)) {
        return {nullptr, std::move(source.source), source.resolvedPath,
                "Shader load failed: " + pathText(source.resolvedPath)};
    }
    return {
        std::move(shader), std::move(source.source), source.resolvedPath, {}};
}

ShaderLoadResult ShaderLoader::load(const std::string& vertexPath,
                                    const std::string& fragmentPath) {
    ShaderSourceResult vertex = readSource(vertexPath);
    if (!vertex) {
        return failedLoad(vertex);
    }
    ShaderSourceResult fragment = readSource(fragmentPath);
    if (!fragment) {
        return failedLoad(fragment);
    }
    std::shared_ptr<sf::Shader> shader = std::make_shared<sf::Shader>();
    if (!shader->loadFromMemory(vertex.source, fragment.source)) {
        return {nullptr,
                {},
                vertex.resolvedPath,
                "Full shader load failed: " + pathText(vertex.resolvedPath) +
                    " and " + pathText(fragment.resolvedPath)};
    }
    return {std::move(shader), {}, vertex.resolvedPath, {}};
}

ShaderLoadResult ShaderLoader::load(const std::string& vertexPath,
                                    const std::string& geometryPath,
                                    const std::string& fragmentPath) {
    ShaderSourceResult vertex = readSource(vertexPath);
    if (!vertex) {
        return failedLoad(vertex);
    }
    ShaderSourceResult geometry = readSource(geometryPath);
    if (!geometry) {
        return failedLoad(geometry);
    }
    ShaderSourceResult fragment = readSource(fragmentPath);
    if (!fragment) {
        return failedLoad(fragment);
    }
    std::shared_ptr<sf::Shader> shader = std::make_shared<sf::Shader>();
    if (!shader->loadFromMemory(vertex.source, geometry.source,
                                fragment.source)) {
        return {nullptr,
                {},
                vertex.resolvedPath,
                "Shader load with geometry failed: " +
                    pathText(vertex.resolvedPath) + ", " +
                    pathText(geometry.resolvedPath) + ", and " +
                    pathText(fragment.resolvedPath)};
    }
    return {std::move(shader), {}, vertex.resolvedPath, {}};
}
