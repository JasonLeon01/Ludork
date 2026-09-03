#include <Compression.hpp>

#include <zlib.h>

#include <array>
#include <limits>
#include <stdexcept>

namespace ludork::standard {

std::vector<std::uint8_t> compressZlib(std::span<const std::uint8_t> bytes,
                                       int level) {
    if (bytes.size() > std::numeric_limits<uLong>::max()) {
        throw std::runtime_error("Zlib input is too large");
    }
    uLongf resultSize = compressBound(static_cast<uLong>(bytes.size()));
    std::vector<std::uint8_t> result(resultSize);
    const int status = compress2(result.data(), &resultSize, bytes.data(),
                                 static_cast<uLong>(bytes.size()), level);
    if (status != Z_OK) {
        throw std::runtime_error("Failed to compress zlib data");
    }
    result.resize(resultSize);
    return result;
}

std::vector<std::uint8_t> decompressZlib(std::span<const std::uint8_t> bytes,
                                         int windowBits) {
    if (bytes.size() > std::numeric_limits<uInt>::max()) {
        throw std::runtime_error("Zlib input is too large");
    }
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(bytes.data());
    stream.avail_in = static_cast<uInt>(bytes.size());
    if (inflateInit2(&stream, windowBits) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib decompression");
    }
    std::vector<std::uint8_t> result;
    std::array<std::uint8_t, 32768> buffer{};
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);
        const std::size_t produced = buffer.size() - stream.avail_out;
        result.insert(result.end(), buffer.begin(), buffer.begin() + produced);
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END) {
        throw std::runtime_error("Failed to decompress zlib data");
    }
    return result;
}

}  // namespace ludork::standard
