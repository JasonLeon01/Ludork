#include <DataFile.hpp>
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
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::standard {

namespace {

constexpr std::array<std::uint8_t, 4> DataMagic = {'L', 'D', 'D', 'C'};
constexpr std::uint8_t DataVersion = 1;
constexpr std::uint8_t DataZlibFlag = 1;
constexpr std::size_t DataHeaderSize = 24;
constexpr std::uint32_t MaximumDataSize = 512U * 1024U * 1024U;
constexpr std::uint64_t KeySeed = 0xD6E8FEB86659FD93ULL;
constexpr std::uint64_t StreamMultiplier = 0x2545F4914F6CDD1DULL;
constexpr std::uint64_t StreamFallback = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t NonceOffset = 0xCBF29CE484222325ULL;
constexpr std::uint64_t NoncePrime = 0x100000001B3ULL;

std::string lowerString(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

bool isRegularFile(const std::filesystem::path& path) {
    std::error_code error;
    const bool regular = std::filesystem::is_regular_file(path, error);
    return regular && !error;
}

bool isEncryptedDataPath(const std::filesystem::path& path) {
    return lowerString(pathToUtf8(path.extension())) == ".ldc";
}

void appendUint32(std::vector<std::uint8_t>& data, std::uint32_t value) {
    for (std::size_t index = 0; index < 4; ++index) {
        data.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
    }
}

void appendUint64(std::vector<std::uint8_t>& data, std::uint64_t value) {
    for (std::size_t index = 0; index < 8; ++index) {
        data.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
    }
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
    if (source.size() > MaximumDataSize) {
        throw std::runtime_error("JSON data is too large: " + pathToUtf8(path));
    }
    uLongf compressedSize = compressBound(static_cast<uLong>(source.size()));
    std::vector<std::uint8_t> compressed(compressedSize);
    const int result =
        compress2(reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
                  reinterpret_cast<const Bytef*>(source.data()),
                  static_cast<uLong>(source.size()), Z_BEST_COMPRESSION);
    if (result != Z_OK) {
        throw std::runtime_error("Failed to compress JSON data: " +
                                 pathToUtf8(path));
    }
    compressed.resize(compressedSize);

    const std::uint64_t nonce = contentNonce(path, source);
    applyStream(compressed, nonce);
    uLong checksum = crc32(0L, Z_NULL, 0);
    checksum = crc32(checksum, reinterpret_cast<const Bytef*>(source.data()),
                     static_cast<uInt>(source.size()));

    std::vector<std::uint8_t> encoded;
    encoded.reserve(DataHeaderSize + compressed.size());
    encoded.insert(encoded.end(), DataMagic.begin(), DataMagic.end());
    encoded.push_back(DataVersion);
    encoded.push_back(DataZlibFlag);
    encoded.push_back(0);
    encoded.push_back(0);
    appendUint32(encoded, static_cast<std::uint32_t>(source.size()));
    appendUint32(encoded, static_cast<std::uint32_t>(checksum));
    appendUint64(encoded, nonce);
    encoded.insert(encoded.end(), compressed.begin(), compressed.end());
    return encoded;
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
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
    if (encoded.size() < DataHeaderSize) {
        throw std::runtime_error("Encrypted data header is truncated: " +
                                 pathToUtf8(path));
    }
    if (!std::equal(DataMagic.begin(), DataMagic.end(), encoded.begin())) {
        throw std::runtime_error("Encrypted data magic is invalid: " +
                                 pathToUtf8(path));
    }
    if (encoded[4] != DataVersion) {
        throw std::runtime_error("Unsupported encrypted data version: " +
                                 std::to_string(encoded[4]) + " in " +
                                 pathToUtf8(path));
    }
    if (encoded[5] != DataZlibFlag || encoded[6] != 0 || encoded[7] != 0) {
        throw std::runtime_error("Encrypted data flags are invalid: " +
                                 pathToUtf8(path));
    }

    const std::uint32_t sourceSize = readUint32(encoded.data() + 8);
    const std::uint32_t expectedChecksum = readUint32(encoded.data() + 12);
    const std::uint64_t nonce = readUint64(encoded.data() + 16);
    if (sourceSize > MaximumDataSize) {
        throw std::runtime_error("Encrypted data source is too large: " +
                                 pathToUtf8(path));
    }

    std::vector<std::uint8_t> compressed(
        encoded.begin() + static_cast<std::ptrdiff_t>(DataHeaderSize),
        encoded.end());
    if (compressed.empty()) {
        throw std::runtime_error("Encrypted data payload is empty: " +
                                 pathToUtf8(path));
    }
    applyStream(compressed, nonce);
    if (compressed.size() > std::numeric_limits<uLong>::max()) {
        throw std::runtime_error("Encrypted data payload is too large: " +
                                 pathToUtf8(path));
    }

    std::string source(std::max<std::size_t>(1, sourceSize), '\0');
    uLongf destinationSize = static_cast<uLongf>(sourceSize);
    const int result =
        uncompress(reinterpret_cast<Bytef*>(source.data()), &destinationSize,
                   reinterpret_cast<const Bytef*>(compressed.data()),
                   static_cast<uLong>(compressed.size()));
    if (result != Z_OK || destinationSize != sourceSize) {
        throw std::runtime_error(
            "Encrypted data payload could not be decompressed: " +
            pathToUtf8(path));
    }
    source.resize(sourceSize);

    uLong checksum = crc32(0L, Z_NULL, 0);
    checksum = crc32(checksum, reinterpret_cast<const Bytef*>(source.data()),
                     static_cast<uInt>(source.size()));
    if (static_cast<std::uint32_t>(checksum) != expectedChecksum) {
        throw std::runtime_error("Encrypted data checksum does not match: " +
                                 pathToUtf8(path));
    }
    return source;
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
