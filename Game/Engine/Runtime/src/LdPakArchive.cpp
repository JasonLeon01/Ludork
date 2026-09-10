#include "LdPakArchive.hpp"

#include <Utf8Path.hpp>

#include <zlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr std::size_t HeaderSize = 40;
constexpr std::size_t EntryHeaderSize = 32;
constexpr std::uint16_t FormatVersion = 1;
constexpr std::uint32_t DirectoryFlag = 1;
constexpr std::size_t CrcBufferSize = 64U * 1024U;

std::uint16_t readU16(const std::uint8_t* value) {
    return static_cast<std::uint16_t>(value[0]) |
           static_cast<std::uint16_t>(value[1]) << 8U;
}

std::uint32_t readU32(const std::uint8_t* value) {
    return static_cast<std::uint32_t>(value[0]) |
           static_cast<std::uint32_t>(value[1]) << 8U |
           static_cast<std::uint32_t>(value[2]) << 16U |
           static_cast<std::uint32_t>(value[3]) << 24U;
}

std::uint64_t readU64(const std::uint8_t* value) {
    std::uint64_t result = 0;
    for (unsigned int index = 0; index < 8; ++index) {
        result |= static_cast<std::uint64_t>(value[index]) << (index * 8U);
    }
    return result;
}

std::uint64_t aligned8(std::uint64_t value) {
    if (value > std::numeric_limits<std::uint64_t>::max() - 7U) {
        throw std::runtime_error("LDPak offset overflow");
    }
    return (value + 7U) & ~std::uint64_t{7U};
}

bool addOverflows(std::uint64_t left, std::uint64_t right) {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
}

bool validUtf8(const std::string_view value) {
    std::size_t index = 0;
    while (index < value.size()) {
        const unsigned char first = static_cast<unsigned char>(value[index++]);
        if (first <= 0x7FU) {
            if (first == 0) {
                return false;
            }
            continue;
        }
        unsigned int trailing = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0U) == 0xC0U) {
            trailing = 1;
            codePoint = first & 0x1FU;
            minimum = 0x80U;
        } else if ((first & 0xF0U) == 0xE0U) {
            trailing = 2;
            codePoint = first & 0x0FU;
            minimum = 0x800U;
        } else if ((first & 0xF8U) == 0xF0U) {
            trailing = 3;
            codePoint = first & 0x07U;
            minimum = 0x10000U;
        } else {
            return false;
        }
        if (index + trailing > value.size()) {
            return false;
        }
        for (unsigned int count = 0; count < trailing; ++count) {
            const unsigned char next =
                static_cast<unsigned char>(value[index++]);
            if ((next & 0xC0U) != 0x80U) {
                return false;
            }
            codePoint = (codePoint << 6U) | (next & 0x3FU);
        }
        if (codePoint < minimum || codePoint > 0x10FFFFU ||
            (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
            return false;
        }
    }
    return true;
}

std::string asciiFold(std::string value) {
    for (char& character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return value;
}

bool isLinkLike(const std::filesystem::path& path,
                const std::filesystem::file_status& status) {
    if (std::filesystem::is_symlink(status)) {
        return true;
    }
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        throw std::runtime_error("Failed to inspect LDPak path");
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    static_cast<void>(path);
    return false;
#endif
}

void validateSegment(const std::string_view segment,
                     const std::string& source) {
    if (segment.empty() || segment == "." || segment == ".." ||
        segment.find('\\') != std::string_view::npos ||
        segment.find('\0') != std::string_view::npos) {
        throw std::runtime_error("Invalid LDPak path: " + source);
    }
}

void validateRelativePath(const std::string_view value,
                          const std::string& source) {
    if (value.empty() || value.front() == '/' || value.back() == '/') {
        throw std::runtime_error("Invalid LDPak path: " + source);
    }
    std::size_t start = 0;
    while (start < value.size()) {
        const std::size_t separator = value.find('/', start);
        const std::size_t end =
            separator == std::string_view::npos ? value.size() : separator;
        validateSegment(value.substr(start, end - start), source);
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1;
    }
}

void validateGroup(const std::string& group) {
    validateSegment(group, group);
    if (group.find('/') != std::string::npos ||
        asciiFold(group).ends_with(".ldpak")) {
        throw std::runtime_error("Invalid LDPak group: " + group);
    }
}

double modificationTime(const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::file_time_type writeTime =
        std::filesystem::last_write_time(path, error);
    if (error) {
        throw std::runtime_error("Failed to read LDPak modification time: " +
                                 error.message());
    }
    struct ClockCalibration {
        std::filesystem::file_time_type fileTime;
        std::chrono::system_clock::time_point systemTime;
    };
    static const ClockCalibration calibration{
        std::filesystem::file_time_type::clock::now(),
        std::chrono::system_clock::now()};
    const std::chrono::system_clock::time_point systemTime =
        calibration.systemTime +
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            writeTime - calibration.fileTime);
    return std::chrono::duration<double>(systemTime.time_since_epoch()).count();
}

std::uint64_t regularFileSize(const std::filesystem::path& path) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error) {
        throw std::runtime_error("Failed to read LDPak file size: " +
                                 error.message());
    }
    if (size > std::numeric_limits<std::uint64_t>::max()) {
        throw std::runtime_error("LDPak file is too large");
    }
    return static_cast<std::uint64_t>(size);
}

void readExact(std::ifstream& stream, void* target, std::size_t size,
               const std::string& description) {
    if (size == 0) {
        return;
    }
    if (size >
        static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error(description + " is too large");
    }
    stream.read(static_cast<char*>(target), static_cast<std::streamsize>(size));
    if (stream.gcount() != static_cast<std::streamsize>(size)) {
        throw std::runtime_error("Truncated " + description);
    }
}

void requireZeroRange(std::ifstream& stream, std::uint64_t offset,
                      std::size_t size, const std::string& description) {
    if (size == 0) {
        return;
    }
    stream.clear();
    stream.seekg(static_cast<std::streamoff>(offset));
    if (!stream) {
        throw std::runtime_error("Failed to seek " + description);
    }
    std::array<std::uint8_t, 7> bytes{};
    readExact(stream, bytes.data(), size, description);
    if (std::any_of(bytes.begin(), bytes.begin() + size,
                    [](std::uint8_t value) {
                        return value != 0;
                    })) {
        throw std::runtime_error(description + " must contain zeros");
    }
}

std::uint32_t bytesCrc(const std::vector<std::uint8_t>& value) {
    uLong checksum = crc32(0L, Z_NULL, 0);
    std::size_t position = 0;
    while (position < value.size()) {
        const std::size_t size = std::min<std::size_t>(
            value.size() - position, std::numeric_limits<uInt>::max());
        checksum =
            crc32(checksum, value.data() + position, static_cast<uInt>(size));
        position += size;
    }
    return static_cast<std::uint32_t>(checksum);
}

std::string parentPath(const std::string& value) {
    const std::size_t separator = value.rfind('/');
    return separator == std::string::npos ? std::string{}
                                          : value.substr(0, separator);
}

}  // namespace

namespace ludork::runtime::detail {

struct LdPakArchive::Impl {
    std::filesystem::path path;
    std::string group;
    double modificationTime = 0.0;
    std::vector<LdPakEntry> entries;
    std::unordered_map<std::string, std::size_t> entryIndices;
};

LdPakArchive::LdPakArchive(const std::filesystem::path& path)
    : impl_(std::make_unique<Impl>()) {
    std::error_code statusError;
    const std::filesystem::file_status status =
        std::filesystem::symlink_status(path, statusError);
    if (statusError || !std::filesystem::is_regular_file(status) ||
        isLinkLike(path, status)) {
        throw std::runtime_error("LDPak file was not found: " +
                                 ludork::standard::pathToUtf8(path));
    }

    const std::uint64_t fileSize = regularFileSize(path);
    if (fileSize < HeaderSize) {
        throw std::runtime_error("LDPak header is truncated: " +
                                 ludork::standard::pathToUtf8(path));
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open LDPak: " +
                                 ludork::standard::pathToUtf8(path));
    }
    std::array<std::uint8_t, HeaderSize> header{};
    readExact(stream, header.data(), header.size(), "LDPak header");
    if (std::memcmp(header.data(), "LDPK", 4) != 0) {
        throw std::runtime_error("Invalid LDPak magic: " +
                                 ludork::standard::pathToUtf8(path));
    }
    const std::uint16_t version = readU16(header.data() + 4);
    const std::uint16_t flags = readU16(header.data() + 6);
    const std::uint32_t groupLength = readU32(header.data() + 8);
    const std::uint32_t entryCount = readU32(header.data() + 12);
    const std::uint64_t indexOffset = readU64(header.data() + 16);
    const std::uint64_t indexSize = readU64(header.data() + 24);
    const std::uint32_t indexCrc = readU32(header.data() + 32);
    const std::uint32_t reserved = readU32(header.data() + 36);
    if (version != FormatVersion || flags != 0 || reserved != 0) {
        throw std::runtime_error("Unsupported LDPak header: " +
                                 ludork::standard::pathToUtf8(path));
    }
    if (groupLength == 0 || addOverflows(HeaderSize, groupLength) ||
        HeaderSize + groupLength > fileSize) {
        throw std::runtime_error("Invalid LDPak group name length");
    }
    std::string group(groupLength, '\0');
    readExact(stream, group.data(), group.size(), "LDPak group name");
    if (!validUtf8(group)) {
        throw std::runtime_error("LDPak group is not valid UTF-8");
    }
    validateGroup(group);
    const std::string expectedFilename = group + ".ldpak";
    if (ludork::standard::pathToUtf8(path.filename()) != expectedFilename) {
        throw std::runtime_error("LDPak filename does not match group " +
                                 group);
    }
    const std::uint64_t dataStart = aligned8(HeaderSize + groupLength);
    const std::size_t groupPadding =
        static_cast<std::size_t>(dataStart - (HeaderSize + groupLength));
    requireZeroRange(stream, HeaderSize + groupLength, groupPadding,
                     "LDPak group padding");
    if (indexOffset < dataStart || indexOffset % 8U != 0 ||
        addOverflows(indexOffset, indexSize) ||
        indexOffset + indexSize != fileSize ||
        entryCount > indexSize / EntryHeaderSize ||
        indexSize > std::numeric_limits<std::size_t>::max() ||
        indexOffset > static_cast<std::uint64_t>(
                          std::numeric_limits<std::streamoff>::max())) {
        throw std::runtime_error("Invalid LDPak index bounds");
    }
    stream.seekg(static_cast<std::streamoff>(indexOffset));
    if (!stream) {
        throw std::runtime_error("Failed to seek LDPak index");
    }
    std::vector<std::uint8_t> index(static_cast<std::size_t>(indexSize));
    readExact(stream, index.data(), index.size(), "LDPak index");
    if (bytesCrc(index) != indexCrc) {
        throw std::runtime_error("LDPak index CRC mismatch: " + group);
    }

    std::vector<LdPakEntry> entries;
    entries.reserve(entryCount);
    std::unordered_set<std::string> directoryPaths;
    std::unordered_map<std::string, std::string> foldedPaths;
    std::size_t position = 0;
    std::string previousPath;
    for (std::uint32_t entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
        if (index.size() - position < EntryHeaderSize) {
            throw std::runtime_error("Truncated LDPak index entry");
        }
        const std::uint8_t* entryHeader = index.data() + position;
        const std::uint32_t pathLength = readU32(entryHeader);
        const std::uint32_t entryFlags = readU32(entryHeader + 4);
        const std::uint64_t dataOffset = readU64(entryHeader + 8);
        const std::uint64_t dataSize = readU64(entryHeader + 16);
        const std::uint32_t crc = readU32(entryHeader + 24);
        const std::uint32_t entryReserved = readU32(entryHeader + 28);
        position += EntryHeaderSize;
        if (pathLength == 0 || pathLength > index.size() - position) {
            throw std::runtime_error("Invalid LDPak entry path length");
        }
        std::string relative(
            reinterpret_cast<const char*>(index.data() + position), pathLength);
        position += pathLength;
        if (!validUtf8(relative)) {
            throw std::runtime_error("LDPak path is not valid UTF-8");
        }
        validateRelativePath(relative, relative);
        if (!previousPath.empty() && previousPath >= relative) {
            throw std::runtime_error(
                "LDPak index paths are not strictly sorted");
        }
        previousPath = relative;
        const std::string folded = asciiFold(relative);
        const auto [foldedIterator, foldedInserted] =
            foldedPaths.emplace(folded, relative);
        if (!foldedInserted && foldedIterator->second != relative) {
            throw std::runtime_error(
                "LDPak paths differ only by case: " + foldedIterator->second +
                " and " + relative);
        }
        if (entryFlags & ~DirectoryFlag || entryReserved != 0) {
            throw std::runtime_error("Unsupported LDPak entry flags: " +
                                     relative);
        }
        const bool directory = (entryFlags & DirectoryFlag) != 0;
        if (directory) {
            if (dataOffset != 0 || dataSize != 0 || crc != 0) {
                throw std::runtime_error(
                    "LDPak directory contains file data: " + relative);
            }
            directoryPaths.insert(relative);
        } else if (dataOffset % 8U != 0 || dataOffset < dataStart ||
                   dataOffset > indexOffset ||
                   addOverflows(dataOffset, dataSize) ||
                   dataOffset + dataSize > indexOffset ||
                   (dataSize == 0 && crc != 0)) {
            throw std::runtime_error("Invalid LDPak data bounds: " + relative);
        }
        entries.push_back({relative, dataOffset, dataSize, crc, directory});
    }
    if (position != index.size()) {
        throw std::runtime_error("LDPak index contains trailing data");
    }
    for (const LdPakEntry& entry : entries) {
        std::string parent = parentPath(entry.path);
        while (!parent.empty()) {
            if (!directoryPaths.contains(parent)) {
                throw std::runtime_error(
                    "LDPak entry has an undeclared parent directory: " +
                    entry.path);
            }
            parent = parentPath(parent);
        }
    }
    std::uint64_t expectedOffset = dataStart;
    for (const LdPakEntry& entry : entries) {
        if (entry.directory) {
            continue;
        }
        const std::uint64_t alignedOffset = aligned8(expectedOffset);
        if (entry.offset != alignedOffset) {
            throw std::runtime_error("LDPak file data is not in index order");
        }
        requireZeroRange(
            stream, expectedOffset,
            static_cast<std::size_t>(alignedOffset - expectedOffset),
            "LDPak file padding");
        expectedOffset = alignedOffset + entry.size;
    }
    const std::uint64_t alignedIndexOffset = aligned8(expectedOffset);
    if (alignedIndexOffset != indexOffset) {
        throw std::runtime_error("LDPak data section size mismatch");
    }
    requireZeroRange(
        stream, expectedOffset,
        static_cast<std::size_t>(alignedIndexOffset - expectedOffset),
        "LDPak index padding");

    impl_->path = path;
    impl_->group = std::move(group);
    impl_->modificationTime = ::modificationTime(path);
    impl_->entries = std::move(entries);
    for (std::size_t entryIndex = 0; entryIndex < impl_->entries.size();
         ++entryIndex) {
        impl_->entryIndices.emplace(impl_->entries[entryIndex].path,
                                    entryIndex);
    }
}

LdPakArchive::~LdPakArchive() = default;
LdPakArchive::LdPakArchive(LdPakArchive&&) noexcept = default;
LdPakArchive& LdPakArchive::operator=(LdPakArchive&&) noexcept = default;

const std::filesystem::path& LdPakArchive::path() const noexcept {
    return impl_->path;
}

const std::string& LdPakArchive::group() const noexcept {
    return impl_->group;
}

double LdPakArchive::modificationTime() const noexcept {
    return impl_->modificationTime;
}

const std::vector<LdPakEntry>& LdPakArchive::entries() const noexcept {
    return impl_->entries;
}

std::vector<std::uint8_t> LdPakArchive::readAll(
    const std::string& relativePath) const {
    const auto iterator = impl_->entryIndices.find(relativePath);
    if (iterator == impl_->entryIndices.end() ||
        impl_->entries[iterator->second].directory) {
        throw std::runtime_error("LDPak file not found: " + relativePath);
    }
    const LdPakEntry& entry = impl_->entries[iterator->second];
    if (entry.size > std::numeric_limits<std::size_t>::max() ||
        entry.offset > static_cast<std::uint64_t>(
                           std::numeric_limits<std::streamoff>::max())) {
        throw std::runtime_error("LDPak entry is too large: " + relativePath);
    }
    std::ifstream stream(impl_->path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open LDPak: " +
                                 ludork::standard::pathToUtf8(impl_->path));
    }
    stream.seekg(static_cast<std::streamoff>(entry.offset));
    if (!stream) {
        throw std::runtime_error("Failed to seek LDPak entry: " + relativePath);
    }
    std::vector<std::uint8_t> result(static_cast<std::size_t>(entry.size));
    uLong checksum = crc32(0L, Z_NULL, 0);
    std::size_t position = 0;
    while (position < result.size()) {
        const std::size_t size = std::min<std::size_t>(
            result.size() - position, std::numeric_limits<uInt>::max());
        readExact(stream, result.data() + position, size,
                  "LDPak entry " + relativePath);
        checksum =
            crc32(checksum, result.data() + position, static_cast<uInt>(size));
        position += size;
    }
    if (static_cast<std::uint32_t>(checksum) != entry.crc) {
        throw std::runtime_error("LDPak data CRC mismatch: " + relativePath);
    }
    return result;
}

std::uint32_t calculateLdPakDataCrc(const std::filesystem::path& path,
                                    std::uint64_t offset, std::uint64_t size) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open LDPak: " +
                                 ludork::standard::pathToUtf8(path));
    }
    if (offset > static_cast<std::uint64_t>(
                     std::numeric_limits<std::streamoff>::max())) {
        throw std::runtime_error("LDPak offset is too large");
    }
    stream.seekg(static_cast<std::streamoff>(offset));
    if (!stream) {
        throw std::runtime_error("Failed to seek LDPak");
    }
    std::array<std::uint8_t, CrcBufferSize> buffer{};
    std::uint64_t remaining = size;
    uLong checksum = crc32(0L, Z_NULL, 0);
    while (remaining > 0) {
        const std::size_t chunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, buffer.size()));
        readExact(stream, buffer.data(), chunk, "LDPak entry");
        checksum = crc32(checksum, buffer.data(), static_cast<uInt>(chunk));
        remaining -= chunk;
    }
    return static_cast<std::uint32_t>(checksum);
}

}  // namespace ludork::runtime::detail
