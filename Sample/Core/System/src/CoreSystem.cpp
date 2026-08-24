#include <CoreSystem.hpp>

#include <Utf8Path.hpp>

#include <zlib.h>

#include <array>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

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

bool exists(const std::string& path) {
    return std::filesystem::exists(ludork::standard::pathFromUtf8(path));
}

std::string currentPath() {
    return ludork::standard::pathToUtf8(std::filesystem::current_path());
}

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
    static constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(((value.size() + 2) / 3) * 4);
    std::size_t index = 0;
    while (index + 3 <= value.size()) {
        const unsigned int first = static_cast<unsigned char>(value[index]);
        const unsigned int second =
            static_cast<unsigned char>(value[index + 1]);
        const unsigned int third = static_cast<unsigned char>(value[index + 2]);
        result.push_back(alphabet[(first >> 2) & 0x3F]);
        result.push_back(alphabet[((first & 0x03) << 4) | (second >> 4)]);
        result.push_back(alphabet[((second & 0x0F) << 2) | (third >> 6)]);
        result.push_back(alphabet[third & 0x3F]);
        index += 3;
    }
    const std::size_t remaining = value.size() - index;
    if (remaining == 1) {
        const unsigned int first = static_cast<unsigned char>(value[index]);
        result.push_back(alphabet[(first >> 2) & 0x3F]);
        result.push_back(alphabet[(first & 0x03) << 4]);
        result.append("==");
    } else if (remaining == 2) {
        const unsigned int first = static_cast<unsigned char>(value[index]);
        const unsigned int second =
            static_cast<unsigned char>(value[index + 1]);
        result.push_back(alphabet[(first >> 2) & 0x3F]);
        result.push_back(alphabet[((first & 0x03) << 4) | (second >> 4)]);
        result.push_back(alphabet[(second & 0x0F) << 2]);
        result.push_back('=');
    }
    return result;
}

std::string decodeBase64(const std::string& value) {
    static constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    unsigned int accumulator = 0;
    int bits = -8;
    for (const unsigned char character : value) {
        if (std::isspace(character)) {
            continue;
        }
        if (character == '=') {
            break;
        }
        const std::size_t index = alphabet.find(static_cast<char>(character));
        if (index == std::string_view::npos) {
            throw std::runtime_error("Invalid base64 data");
        }
        accumulator = (accumulator << 6) | static_cast<unsigned int>(index);
        bits += 6;
        if (bits >= 0) {
            result.push_back(static_cast<char>((accumulator >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return result;
}
