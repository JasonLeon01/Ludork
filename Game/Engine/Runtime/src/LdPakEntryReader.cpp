#include "LdPakEntryReader.hpp"
#include <LudorkGenerated/LdPakFormatConstants.hpp>
#include <zlib.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace format = ludork::generated::ldpak;

namespace ludork::runtime::detail {

LdPakEntryReader::LdPakEntryReader(const std::filesystem::path& source,
                                   const LdPakEntry& entry)
    : entry_(entry), stream_(source, std::ios::binary) {
    if (!stream_) {
        throw std::runtime_error("Failed to open LDPak entry: " + entry.path);
    }
}

void LdPakEntryReader::loadBlock(std::size_t index) {
    if (cachedBlock_ == index) {
        return;
    }
    cachedBlock_ = std::numeric_limits<std::size_t>::max();
    const LdPakEntry::Block& block = entry_.blocks.at(index);
    const std::size_t size = static_cast<std::size_t>(std::min<std::uint64_t>(
        format::BlockSize,
        entry_.size - static_cast<std::uint64_t>(index) * format::BlockSize));
    if (block.offset > static_cast<std::uint64_t>(
                           std::numeric_limits<std::streamoff>::max())) {
        throw std::runtime_error("LDPak block offset is too large: " +
                                 entry_.path);
    }
    stream_.clear();
    stream_.seekg(static_cast<std::streamoff>(block.offset));
    std::vector<std::uint8_t> stored(block.storedSize);
    stream_.read(reinterpret_cast<char*>(stored.data()), block.storedSize);
    if (!stream_ || stream_.gcount() != block.storedSize) {
        throw std::runtime_error("Truncated LDPak block: " + entry_.path);
    }
    if (block.flags == format::BlockZlibFlag) {
        buffer_.resize(size);
        z_stream decoder{};
        decoder.next_in = stored.data();
        decoder.avail_in = static_cast<uInt>(stored.size());
        decoder.next_out = buffer_.data();
        decoder.avail_out = static_cast<uInt>(buffer_.size());
        if (inflateInit(&decoder) != Z_OK) {
            throw std::runtime_error(
                "Failed to initialise LDPak decompression");
        }
        const int status = inflate(&decoder, Z_FINISH);
        const bool valid = status == Z_STREAM_END && decoder.avail_in == 0 &&
                           decoder.total_out == size;
        inflateEnd(&decoder);
        if (!valid) {
            throw std::runtime_error("Invalid LDPak compressed block: " +
                                     entry_.path);
        }
    } else {
        buffer_ = std::move(stored);
    }
    if (static_cast<std::uint32_t>(
            crc32(0L, buffer_.data(), static_cast<uInt>(buffer_.size()))) !=
        block.crc) {
        throw std::runtime_error("LDPak block CRC mismatch: " + entry_.path);
    }
    cachedBlock_ = index;
}

void LdPakEntryReader::read(std::uint64_t position, void* data,
                            std::size_t size) {
    if (position > entry_.size || size > entry_.size - position ||
        (data == nullptr && size != 0)) {
        throw std::runtime_error("Invalid LDPak read range: " + entry_.path);
    }
    std::uint64_t checkedPosition = checkedPosition_;
    std::uint32_t checksum = checksum_;
    auto* output = static_cast<std::uint8_t*>(data);
    if (position == 0) {
        checkedPosition = 0;
        checksum = 0;
    }
    if (position != checkedPosition) {
        checkedPosition = std::numeric_limits<std::uint64_t>::max();
    }
    while (size != 0) {
        loadBlock(static_cast<std::size_t>(position / format::BlockSize));
        const std::size_t offset =
            static_cast<std::size_t>(position % format::BlockSize);
        const std::size_t count = std::min(size, buffer_.size() - offset);
        std::memcpy(output, buffer_.data() + offset, count);
        if (checkedPosition == position) {
            checksum = static_cast<std::uint32_t>(
                crc32(checksum, output, static_cast<uInt>(count)));
            checkedPosition += count;
        }
        output += count;
        position += count;
        size -= count;
    }
    if (checkedPosition == entry_.size && checksum != entry_.crc) {
        throw std::runtime_error("LDPak data CRC mismatch: " + entry_.path);
    }
    checkedPosition_ = checkedPosition;
    checksum_ = checksum;
}

std::vector<std::uint8_t> LdPakEntryReader::readAll() {
    if (entry_.size > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("LDPak entry is too large: " + entry_.path);
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(entry_.size));
    read(0, result.data(), result.size());
    return result;
}

}  // namespace ludork::runtime::detail
