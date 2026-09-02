#include <CoreSystem.hpp>

#include <Base64.hpp>
#include <Utf8Path.hpp>

#include <zlib.h>

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {
std::string transform(const std::string& value, int windowBits) {
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(value.data()));
    stream.avail_in = static_cast<uInt>(value.size());
    if (inflateInit2(&stream, windowBits) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib decompression");
    }
    std::string result;
    std::array<char, 32768> buffer{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        result.append(buffer.data(), buffer.size() - stream.avail_out);
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END) {
        throw std::runtime_error("Failed to decompress zlib data");
    }
    return result;
}
}  // namespace

void createDirectories(const std::string& path) {
    std::filesystem::create_directories(ludork::standard::pathFromUtf8(path));
}

void removeFile(const std::string& path) {
    if (!std::filesystem::remove(ludork::standard::pathFromUtf8(path))) {
        throw std::runtime_error("Failed to remove file: " + path);
    }
}

std::string compress(const std::string& value) {
    uLongf resultSize = compressBound(static_cast<uLong>(value.size()));
    std::string result(resultSize, '\0');
    const int status =
        compress2(reinterpret_cast<Bytef*>(result.data()), &resultSize,
                  reinterpret_cast<const Bytef*>(value.data()),
                  static_cast<uLong>(value.size()), Z_DEFAULT_COMPRESSION);
    if (status != Z_OK) {
        throw std::runtime_error("Failed to compress zlib data");
    }
    result.resize(resultSize);
    return result;
}

std::string decompress(const std::string& value) {
    return transform(value, 15 + 32);
}

std::string encodeBase64(const std::string& value) {
    return ludork::standard::encodeBase64(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size()));
}

std::string decodeBase64(const std::string& value) {
    try {
        const std::vector<std::uint8_t> bytes =
            ludork::standard::decodeBase64(value);
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    } catch (const std::invalid_argument&) {
        throw std::runtime_error("Invalid base64 data");
    }
}
