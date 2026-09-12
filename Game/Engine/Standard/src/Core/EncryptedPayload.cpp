#include <EncryptedPayload.hpp>
#include <LudorkGenerated/EncryptedPayloadConstants.hpp>

#include <zlib.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ludork::standard {
namespace format = ludork::generated::encrypted;

namespace {

std::string message(std::string_view prefix, std::string_view context) {
    return std::string(prefix) + ": " + std::string(context);
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
    std::uint64_t state = nonce ^ format::KeySeed;
    if (state == 0) {
        state = format::StreamFallback;
    }
    std::size_t offset = 0;
    while (offset < data.size()) {
        state ^= state >> format::StreamRightShiftFirst;
        state ^= state << format::StreamLeftShift;
        state ^= state >> format::StreamRightShiftLast;
        const std::uint64_t block = state * format::StreamMultiplier;
        const std::size_t count = std::min<std::size_t>(format::StreamBlockSize,
                                                        data.size() - offset);
        for (std::size_t index = 0; index < count; ++index) {
            data[offset + index] ^=
                static_cast<std::uint8_t>(block >> (index * 8U));
        }
        offset += count;
    }
}

std::string encryptedPrefix(const EncryptedPayloadFormat& format) {
    return "Encrypted " + std::string(format.formatName);
}

}  // namespace

std::vector<std::uint8_t> encodeEncryptedPayload(
    std::string_view source, std::uint64_t nonce,
    const EncryptedPayloadFormat& format, std::string_view context,
    std::string_view sourceName) {
    if (source.size() > format.maximumSourceSize) {
        throw std::runtime_error(
            message(std::string(sourceName) + " is too large", context));
    }
    uLongf compressedSize = compressBound(static_cast<uLong>(source.size()));
    std::vector<std::uint8_t> compressed(compressedSize);
    const int result =
        compress2(compressed.data(), &compressedSize,
                  reinterpret_cast<const Bytef*>(source.data()),
                  static_cast<uLong>(source.size()), Z_BEST_COMPRESSION);
    if (result != Z_OK) {
        throw std::runtime_error(
            message("Failed to compress " + std::string(sourceName), context));
    }
    compressed.resize(compressedSize);
    applyStream(compressed, nonce);

    uLong checksum = crc32(0L, Z_NULL, 0);
    checksum = crc32(checksum, reinterpret_cast<const Bytef*>(source.data()),
                     static_cast<uInt>(source.size()));

    std::vector<std::uint8_t> encoded;
    encoded.reserve(format::HeaderSize + compressed.size());
    encoded.insert(encoded.end(), format.magic.begin(), format.magic.end());
    encoded.push_back(format::Version);
    encoded.push_back(format::ZlibFlag);
    encoded.push_back(0);
    encoded.push_back(0);
    appendUint32(encoded, static_cast<std::uint32_t>(source.size()));
    appendUint32(encoded, static_cast<std::uint32_t>(checksum));
    appendUint64(encoded, nonce);
    encoded.insert(encoded.end(), compressed.begin(), compressed.end());
    return encoded;
}

std::string decodeEncryptedPayload(std::span<const std::uint8_t> encoded,
                                   const EncryptedPayloadFormat& format,
                                   std::string_view context) {
    const std::string prefix = encryptedPrefix(format);
    if (encoded.size() < format::HeaderSize) {
        throw std::runtime_error(
            message(prefix + " header is truncated", context));
    }
    if (!std::equal(format.magic.begin(), format.magic.end(),
                    encoded.begin())) {
        throw std::runtime_error(
            message(prefix + " magic is invalid", context));
    }
    if (encoded[format::HeaderVersionOffset] != format::Version) {
        throw std::runtime_error(
            "Unsupported encrypted " + std::string(format.formatName) +
            " version: " +
            std::to_string(encoded[format::HeaderVersionOffset]) + " in " +
            std::string(context));
    }
    if (encoded[format::HeaderFlagsOffset] != format::ZlibFlag ||
        encoded[format::HeaderReservedOffset] != 0 ||
        encoded[format::HeaderReservedOffset + 1] != 0) {
        throw std::runtime_error(
            message(prefix + " flags are invalid", context));
    }

    const std::uint32_t sourceSize =
        readUint32(encoded.data() + format::HeaderSourceSizeOffset);
    const std::uint32_t expectedChecksum =
        readUint32(encoded.data() + format::HeaderChecksumOffset);
    const std::uint64_t nonce =
        readUint64(encoded.data() + format::HeaderNonceOffset);
    if (sourceSize > format.maximumSourceSize) {
        throw std::runtime_error(
            message(prefix + " source is too large", context));
    }

    std::vector<std::uint8_t> compressed(encoded.begin() + format::HeaderSize,
                                         encoded.end());
    if (compressed.empty()) {
        throw std::runtime_error(
            message(prefix + " payload is empty", context));
    }
    applyStream(compressed, nonce);
    if (compressed.size() > std::numeric_limits<uLong>::max()) {
        throw std::runtime_error(
            message(prefix + " payload is too large", context));
    }

    std::string source(std::max<std::size_t>(1, sourceSize), '\0');
    uLongf destinationSize = static_cast<uLongf>(sourceSize);
    const int result =
        uncompress(reinterpret_cast<Bytef*>(source.data()), &destinationSize,
                   compressed.data(), static_cast<uLong>(compressed.size()));
    if (result != Z_OK || destinationSize != sourceSize) {
        throw std::runtime_error(
            message(prefix + " payload could not be decompressed", context));
    }
    source.resize(sourceSize);

    uLong checksum = crc32(0L, Z_NULL, 0);
    checksum = crc32(checksum, reinterpret_cast<const Bytef*>(source.data()),
                     static_cast<uInt>(source.size()));
    if (static_cast<std::uint32_t>(checksum) != expectedChecksum) {
        throw std::runtime_error(
            message(prefix + " checksum does not match", context));
    }
    return source;
}

}  // namespace ludork::standard
