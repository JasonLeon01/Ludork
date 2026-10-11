#include <DataFile.hpp>
#include <StringUtils.hpp>
#include <LudorkGenerated/EncryptedPayloadConstants.hpp>
#include <LudorkGenerated/ResourceFileConstants.hpp>
#include <EncryptedPayload.hpp>
#include <ReadOnlyFileProvider.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::standard {

namespace {

constexpr std::uint64_t NonceOffset = 0xCBF29CE484222325ULL;
constexpr std::uint64_t NoncePrime = 0x100000001B3ULL;
constexpr EncryptedPayloadFormat DataFormat{
    .magic = ludork::generated::encrypted::DataMagic,
    .maximumSourceSize = ludork::generated::encrypted::MaximumDataSize,
    .formatName = "data",
};

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
    return ludork::standard::lowercase(pathToUtf8(path.extension())) ==
           ludork::generated::resources::EncryptedDataExtension;
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

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    const ReadOnlyFileStatus virtualStatus = readOnlyFileStatus(path);
    if (virtualStatus.handled) {
        if (virtualStatus.type != ReadOnlyFileType::Regular) {
            throw std::runtime_error("Failed to open data file: " +
                                     pathToUtf8(path));
        }
        return readOnlyFileBytes(path);
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open data file: " +
                                 pathToUtf8(path));
    }
    std::vector<std::uint8_t> contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Failed to read data file: " +
                                 pathToUtf8(path));
    }
    return contents;
}

}  // namespace

std::filesystem::path resolveDataPath(const std::filesystem::path& path) {
    if (isRegularFile(path)) {
        return path;
    }
    if (ludork::standard::lowercase(pathToUtf8(path.extension())) ==
        ludork::generated::resources::DataExtension) {
        std::filesystem::path encrypted = path;
        encrypted.replace_extension(
            ludork::generated::resources::EncryptedDataExtension);
        if (isRegularFile(encrypted)) {
            return encrypted;
        }
    }
    return path;
}

std::filesystem::path logicalDataPath(const std::filesystem::path& path) {
    if (!isEncryptedDataPath(path)) {
        return path;
    }
    std::filesystem::path logical = path;
    logical.replace_extension(ludork::generated::resources::DataExtension);
    return logical;
}

bool dataFileExists(const std::filesystem::path& path) {
    return isRegularFile(resolveDataPath(path));
}

bool fileExists(const std::filesystem::path& path) {
    return isRegularFile(path);
}

std::string readFileBytes(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> contents = readFile(path);
    return {contents.begin(), contents.end()};
}

void writeFileBytes(const std::filesystem::path& path, std::string_view bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open data file for writing: " +
                                 pathToUtf8(path));
    }
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("Failed to write data file: " +
                                 pathToUtf8(path));
    }
}

bool isLdcData(std::string_view bytes) {
    return bytes.size() >= DataFormat.magic.size() &&
           std::equal(DataFormat.magic.begin(), DataFormat.magic.end(),
                      bytes.begin());
}

std::string decodeDataPayload(std::string_view bytes,
                              std::string_view context) {
    return decodeEncryptedPayload(
        {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()},
        DataFormat, context);
}

std::string encodeDataPayload(const std::filesystem::path& path,
                              const std::string& payload) {
    const std::vector<std::uint8_t> encoded =
        encodeEncryptedPayload(payload, contentNonce(path, payload), DataFormat,
                               pathToUtf8(path), "binary data");
    return {encoded.begin(), encoded.end()};
}

}  // namespace ludork::standard
