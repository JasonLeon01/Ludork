#include <Utils/ShaderLoader.hpp>

#include <Utf8Path.hpp>

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::array<std::uint8_t, 4> ShaderMagic = {'L', 'D', 'S', 'C'};
constexpr std::uint8_t ShaderVersion = 1;
constexpr std::uint8_t ShaderZlibFlag = 1;
constexpr std::size_t ShaderHeaderSize = 24;
constexpr std::uint32_t MaximumShaderSize = 64U * 1024U * 1024U;
constexpr std::uint64_t KeySeed = 0xD6E8FEB86659FD93ULL;
constexpr std::uint64_t StreamMultiplier = 0x2545F4914F6CDD1DULL;
constexpr std::uint64_t StreamFallback = 0x9E3779B97F4A7C15ULL;

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

std::uint32_t readUint32(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0]) |
           static_cast<std::uint32_t>(data[1]) << 8U |
           static_cast<std::uint32_t>(data[2]) << 16U |
           static_cast<std::uint32_t>(data[3]) << 24U;
}

std::uint64_t readUint64(const std::uint8_t* data) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index) {
        value |= static_cast<std::uint64_t>(data[index]) << (index * 8U);
    }
    return value;
}

void applyStream(std::vector<std::uint8_t>& data, std::uint64_t nonce) {
    std::uint64_t state = nonce ^ KeySeed;
    if (state == 0) {
        state = StreamFallback;
    }
    std::size_t offset = 0;
    while (offset < data.size()) {
        state ^= state >> 12U;
        state ^= state << 25U;
        state ^= state >> 27U;
        const std::uint64_t block = state * StreamMultiplier;
        const std::size_t count =
            std::min<std::size_t>(8, data.size() - offset);
        for (std::size_t index = 0; index < count; ++index) {
            data[offset + index] ^=
                static_cast<std::uint8_t>(block >> (index * 8U));
        }
        offset += count;
    }
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
    if (encoded.size() < ShaderHeaderSize) {
        return {{},
                path,
                "Encrypted shader header is truncated: " + pathText(path)};
    }
    if (!std::equal(ShaderMagic.begin(), ShaderMagic.end(), encoded.begin())) {
        return {
            {}, path, "Encrypted shader magic is invalid: " + pathText(path)};
    }
    if (encoded[4] != ShaderVersion) {
        return {{},
                path,
                "Unsupported encrypted shader version: " +
                    std::to_string(encoded[4]) + " in " + pathText(path)};
    }
    if (encoded[5] != ShaderZlibFlag || encoded[6] != 0 || encoded[7] != 0) {
        return {
            {}, path, "Encrypted shader flags are invalid: " + pathText(path)};
    }

    const std::uint32_t sourceSize = readUint32(encoded.data() + 8);
    const std::uint32_t expectedChecksum = readUint32(encoded.data() + 12);
    const std::uint64_t nonce = readUint64(encoded.data() + 16);
    if (sourceSize > MaximumShaderSize) {
        return {{},
                path,
                "Encrypted shader source is too large: " + pathText(path)};
    }

    std::vector<std::uint8_t> compressed(
        encoded.begin() + static_cast<std::ptrdiff_t>(ShaderHeaderSize),
        encoded.end());
    if (compressed.empty()) {
        return {
            {}, path, "Encrypted shader payload is empty: " + pathText(path)};
    }
    applyStream(compressed, nonce);
    if (compressed.size() > std::numeric_limits<uLong>::max()) {
        return {{},
                path,
                "Encrypted shader payload is too large: " + pathText(path)};
    }

    std::string source(std::max<std::size_t>(1, sourceSize), '\0');
    uLongf destinationSize = static_cast<uLongf>(sourceSize);
    const int result =
        uncompress(reinterpret_cast<Bytef*>(source.data()), &destinationSize,
                   reinterpret_cast<const Bytef*>(compressed.data()),
                   static_cast<uLong>(compressed.size()));
    if (result != Z_OK || destinationSize != sourceSize) {
        return {{},
                path,
                "Encrypted shader payload could not be decompressed: " +
                    pathText(path)};
    }
    source.resize(sourceSize);

    uLong checksum = crc32(0L, Z_NULL, 0);
    checksum = crc32(checksum, reinterpret_cast<const Bytef*>(source.data()),
                     static_cast<uInt>(source.size()));
    if (static_cast<std::uint32_t>(checksum) != expectedChecksum) {
        return {{},
                path,
                "Encrypted shader checksum does not match: " + pathText(path)};
    }
    return {std::move(source), path, {}};
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
