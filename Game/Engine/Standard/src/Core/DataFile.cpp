#include <DataFile.hpp>
#include <EncryptedPayload.hpp>
#include <ReadOnlyFileProvider.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::standard {

namespace {

constexpr std::uint32_t MaximumDataSize = 512U * 1024U * 1024U;
constexpr std::uint64_t NonceOffset = 0xCBF29CE484222325ULL;
constexpr std::uint64_t NoncePrime = 0x100000001B3ULL;
constexpr EncryptedPayloadFormat DataFormat{
    .magic = {'L', 'D', 'D', 'C'},
    .maximumSourceSize = MaximumDataSize,
    .formatName = "data",
};

std::string lowerString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool isRegularFile(const std::filesystem::path& path) {
    const ReadOnlyFileStatus virtualStatus = readOnlyFileStatus(path);
    if (virtualStatus.handled) {
        return virtualStatus.type == ReadOnlyFileType::Regular;
    }
    std::error_code error;
    const bool regular = std::filesystem::is_regular_file(path, error);
    return regular && !error;
}

bool isEncryptedDataPath(const std::filesystem::path& path) {
    return lowerString(pathToUtf8(path.extension())) == ".ldc";
}

std::uint64_t contentNonce(const std::filesystem::path& path,
                           const std::string& source) {
    std::uint64_t result = NonceOffset;
    const std::string pathValue = pathToGenericUtf8(path);
    for (const unsigned char value : pathValue) {
        result ^= value;
        result *= NoncePrime;
    }
    result *= NoncePrime;
    for (const unsigned char value : source) {
        result ^= value;
        result *= NoncePrime;
    }
    return result;
}

std::vector<std::uint8_t> encodeData(const std::filesystem::path& path,
                                     const std::string& source) {
    const std::uint64_t nonce = contentNonce(path, source);
    return encodeEncryptedPayload(source, nonce, DataFormat, pathToUtf8(path),
                                  "JSON data");
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    const ReadOnlyFileStatus virtualStatus = readOnlyFileStatus(path);
    if (virtualStatus.handled) {
        if (virtualStatus.type != ReadOnlyFileType::Regular) {
            throw std::runtime_error("Failed to open JSON file: " +
                                     pathToUtf8(path));
        }
        return readOnlyFileBytes(path);
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open JSON file: " +
                                 pathToUtf8(path));
    }
    std::vector<std::uint8_t> contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Failed to read JSON file: " +
                                 pathToUtf8(path));
    }
    return contents;
}

std::string decodeData(const std::filesystem::path& path,
                       const std::vector<std::uint8_t>& encoded) {
    return decodeEncryptedPayload(encoded, DataFormat, pathToUtf8(path));
}

}  // namespace

std::filesystem::path resolveJsonDataPath(const std::filesystem::path& path) {
    if (isRegularFile(path)) {
        return path;
    }
    if (lowerString(pathToUtf8(path.extension())) == ".json") {
        std::filesystem::path encrypted = path;
        encrypted.replace_extension(".ldc");
        if (isRegularFile(encrypted)) {
            return encrypted;
        }
    }
    return path;
}

std::filesystem::path logicalJsonDataPath(const std::filesystem::path& path) {
    if (!isEncryptedDataPath(path)) {
        return path;
    }
    std::filesystem::path logical = path;
    logical.replace_extension(".json");
    return logical;
}

bool jsonDataExists(const std::filesystem::path& path) {
    return isRegularFile(resolveJsonDataPath(path));
}

std::string readJsonText(const std::filesystem::path& path) {
    const std::filesystem::path resolved = resolveJsonDataPath(path);
    if (!isRegularFile(resolved)) {
        throw std::runtime_error("Failed to open JSON file: " +
                                 pathToUtf8(path));
    }
    const std::vector<std::uint8_t> contents = readFile(resolved);
    if (isEncryptedDataPath(resolved)) {
        return decodeData(resolved, contents);
    }
    return {contents.begin(), contents.end()};
}

void writeJsonText(const std::filesystem::path& path,
                   const std::string& source) {
    const bool encrypted = isEncryptedDataPath(path);
    const std::vector<std::uint8_t> encoded =
        encrypted ? encodeData(path, source) : std::vector<std::uint8_t>{};
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open JSON file for writing: " +
                                 pathToUtf8(path));
    }
    if (encrypted) {
        output.write(reinterpret_cast<const char*>(encoded.data()),
                     static_cast<std::streamsize>(encoded.size()));
    } else {
        output.write(source.data(),
                     static_cast<std::streamsize>(source.size()));
    }
    if (!output) {
        throw std::runtime_error("Failed to write JSON file: " +
                                 pathToUtf8(path));
    }
}

}  // namespace ludork::standard
