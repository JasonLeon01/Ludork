#include <Runtime/AssetStore.hpp>

#include "LdPakArchive.hpp"
#include <Runtime/AssetPath.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
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

bool addOverflows(std::uint64_t left, std::uint64_t right) {
    return right > std::numeric_limits<std::uint64_t>::max() - left;
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
        throw std::runtime_error("Failed to inspect asset filesystem entry");
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    static_cast<void>(path);
    return false;
#endif
}

bool isIgnoredAssetMetadata(const std::filesystem::path& path,
                            const std::filesystem::file_status& status) {
    return std::filesystem::is_regular_file(status) &&
           path.filename() == ".DS_Store";
}

bool isWithin(const std::filesystem::path& root,
              const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::path canonicalRoot =
        std::filesystem::weakly_canonical(root, error);
    if (error) {
        return false;
    }
    const std::filesystem::path canonicalPath =
        std::filesystem::weakly_canonical(path, error);
    if (error) {
        return false;
    }
    const std::filesystem::path relative =
        canonicalPath.lexically_relative(canonicalRoot);
    if (relative.empty() || relative.is_absolute()) {
        return false;
    }
    return std::none_of(relative.begin(), relative.end(),
                        [](const std::filesystem::path& part) {
                            return part == "..";
                        });
}

double modificationTime(const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::file_time_type writeTime =
        std::filesystem::last_write_time(path, error);
    if (error) {
        throw std::runtime_error("Failed to read asset modification time: " +
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
        throw std::runtime_error("Failed to read asset file size: " +
                                 error.message());
    }
    if (size > std::numeric_limits<std::uint64_t>::max()) {
        throw std::runtime_error("Asset file is too large");
    }
    return static_cast<std::uint64_t>(size);
}

}  // namespace

namespace ludork::runtime {

struct AssetInputStream::Impl {
    std::ifstream stream;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint64_t position = 0;
};

AssetInputStream::AssetInputStream(const std::filesystem::path& source,
                                   std::uint64_t offset, std::uint64_t size)
    : impl_(std::make_unique<Impl>()) {
    if (offset > static_cast<std::uint64_t>(
                     std::numeric_limits<std::streamoff>::max()) ||
        size > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("Asset stream is too large for this platform");
    }
    impl_->stream.open(source, std::ios::binary);
    if (!impl_->stream) {
        throw std::runtime_error("Failed to open asset source: " +
                                 ludork::standard::pathToUtf8(source));
    }
    impl_->stream.seekg(static_cast<std::streamoff>(offset));
    if (!impl_->stream) {
        throw std::runtime_error("Failed to seek asset source");
    }
    impl_->offset = offset;
    impl_->size = size;
}

AssetInputStream::~AssetInputStream() = default;
AssetInputStream::AssetInputStream(AssetInputStream&&) noexcept = default;
AssetInputStream& AssetInputStream::operator=(AssetInputStream&&) noexcept =
    default;

std::optional<std::size_t> AssetInputStream::read(void* data,
                                                  std::size_t size) {
    if (impl_ == nullptr || (data == nullptr && size != 0)) {
        return std::nullopt;
    }
    const std::uint64_t remaining = impl_->size - impl_->position;
    const std::size_t requested =
        static_cast<std::size_t>(std::min<std::uint64_t>(remaining, size));
    if (requested == 0) {
        return 0;
    }
    if (requested >
        static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        return std::nullopt;
    }
    impl_->stream.read(static_cast<char*>(data),
                       static_cast<std::streamsize>(requested));
    const std::streamsize count = impl_->stream.gcount();
    if (count != static_cast<std::streamsize>(requested)) {
        return std::nullopt;
    }
    impl_->position += static_cast<std::uint64_t>(count);
    return static_cast<std::size_t>(count);
}

std::optional<std::size_t> AssetInputStream::seek(std::size_t position) {
    if (impl_ == nullptr || position > impl_->size ||
        addOverflows(impl_->offset, position) ||
        impl_->offset + position >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::streamoff>::max())) {
        return std::nullopt;
    }
    impl_->stream.clear();
    impl_->stream.seekg(static_cast<std::streamoff>(impl_->offset + position));
    if (!impl_->stream) {
        return std::nullopt;
    }
    impl_->position = position;
    return position;
}

std::optional<std::size_t> AssetInputStream::tell() {
    return impl_ == nullptr ? std::nullopt
                            : std::optional<std::size_t>(
                                  static_cast<std::size_t>(impl_->position));
}

std::optional<std::size_t> AssetInputStream::getSize() {
    return impl_ == nullptr ? std::nullopt
                            : std::optional<std::size_t>(
                                  static_cast<std::size_t>(impl_->size));
}

namespace {

struct StoreEntry {
    std::filesystem::path source;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint32_t crc = 0;
    double modificationTime = 0.0;
    bool directory = false;
    bool packed = false;
};

}  // namespace

struct AssetStore::Impl {
    mutable std::shared_mutex mutex;
    mutable std::mutex validationMutex;
    std::filesystem::path runtimeRoot;
    AssetStoreMode mode = AssetStoreMode::Loose;
    bool configured = false;
    std::unordered_map<std::string, StoreEntry> entries;
    mutable std::unordered_set<std::string> validatedEntries;
};

namespace {

void addEntry(std::unordered_map<std::string, StoreEntry>& entries,
              std::unordered_map<std::string, std::string>& foldedPaths,
              const std::string& key, StoreEntry entry) {
    if (!entries.emplace(key, std::move(entry)).second) {
        throw std::runtime_error("Duplicate asset path: " + key);
    }
    const std::string folded = asciiFold(key);
    const auto [iterator, inserted] = foldedPaths.emplace(folded, key);
    if (!inserted && iterator->second != key) {
        throw std::runtime_error("Asset paths differ only by case: " +
                                 iterator->second + " and " + key);
    }
}

std::optional<StoreEntry> findLooseEntry(
    const std::filesystem::path& assetsRoot, const AssetPath& assetPath) {
    std::vector<std::string> segments;
    segments.push_back(assetPath.group);
    std::size_t start = 0;
    while (start < assetPath.relativePath.size()) {
        const std::size_t separator = assetPath.relativePath.find('/', start);
        const std::size_t end = separator == std::string::npos
                                    ? assetPath.relativePath.size()
                                    : separator;
        segments.push_back(assetPath.relativePath.substr(start, end - start));
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }

    std::filesystem::path current = assetsRoot;
    for (std::size_t index = 0; index < segments.size(); ++index) {
        std::error_code error;
        std::filesystem::directory_iterator iterator(current, error);
        if (error) {
            if (error == std::errc::no_such_file_or_directory ||
                error == std::errc::not_a_directory) {
                return std::nullopt;
            }
            throw std::runtime_error("Failed to inspect loose asset path: " +
                                     error.message());
        }
        const std::filesystem::directory_iterator end;
        std::optional<std::filesystem::directory_entry> matched;
        while (iterator != end) {
            const std::filesystem::directory_entry entry = *iterator;
            if (ludork::standard::pathToUtf8(entry.path().filename()) ==
                segments[index]) {
                matched = entry;
                break;
            }
            iterator.increment(error);
            if (error) {
                throw std::runtime_error(
                    "Failed to inspect loose asset path: " + error.message());
            }
        }
        if (!matched.has_value()) {
            return std::nullopt;
        }
        const std::filesystem::file_status status =
            matched->symlink_status(error);
        if (error) {
            throw std::runtime_error("Failed to inspect loose asset: " +
                                     error.message());
        }
        if (isLinkLike(matched->path(), status)) {
            throw std::runtime_error("Asset symlinks are not supported: " +
                                     assetPath.value);
        }
        if (isIgnoredAssetMetadata(matched->path(), status)) {
            return std::nullopt;
        }
        const bool directory = std::filesystem::is_directory(status);
        if (!directory && !std::filesystem::is_regular_file(status)) {
            throw std::runtime_error("Unsupported loose asset entry: " +
                                     assetPath.value);
        }
        if (index + 1 < segments.size() && !directory) {
            return std::nullopt;
        }
        current = matched->path();
        if (!isWithin(assetsRoot, current)) {
            throw std::runtime_error("Asset path escapes Assets: " +
                                     assetPath.value);
        }
        if (index + 1 == segments.size()) {
            return StoreEntry{current,
                              0,
                              directory ? 0 : regularFileSize(current),
                              0,
                              modificationTime(current),
                              directory,
                              false};
        }
    }
    return std::nullopt;
}

void loadLooseGroup(const std::filesystem::path& assetsRoot,
                    const std::filesystem::directory_entry& groupEntry,
                    std::unordered_map<std::string, StoreEntry>& entries,
                    std::unordered_map<std::string, std::string>& foldedPaths) {
    const std::string group =
        ludork::standard::pathToUtf8(groupEntry.path().filename());
    static_cast<void>(makeAssetPath(group, "validation"));
    const std::string groupKey = "/Game/Assets/" + group;
    addEntry(entries, foldedPaths, groupKey,
             {groupEntry.path(), 0, 0, 0, modificationTime(groupEntry.path()),
              true, false});

    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        groupEntry.path(), std::filesystem::directory_options::none, error);
    if (error) {
        throw std::runtime_error("Failed to enumerate asset group " + group +
                                 ": " + error.message());
    }
    const std::filesystem::recursive_directory_iterator end;
    while (iterator != end) {
        const std::filesystem::directory_entry entry = *iterator;
        const std::filesystem::file_status status = entry.symlink_status(error);
        if (error) {
            throw std::runtime_error("Failed to inspect loose asset: " +
                                     error.message());
        }
        if (isLinkLike(entry.path(), status)) {
            throw std::runtime_error(
                "Asset symlinks are not supported: " +
                ludork::standard::pathToUtf8(entry.path()));
        }
        if (!isIgnoredAssetMetadata(entry.path(), status)) {
            const bool directory = std::filesystem::is_directory(status);
            if (!directory && !std::filesystem::is_regular_file(status)) {
                throw std::runtime_error(
                    "Unsupported loose asset entry: " +
                    ludork::standard::pathToUtf8(entry.path()));
            }
            const std::filesystem::path relative =
                entry.path().lexically_relative(assetsRoot);
            const std::string relativeText =
                ludork::standard::pathToGenericUtf8(relative);
            const std::string key = "/Game/Assets/" + relativeText;
            static_cast<void>(AssetPath::parse(key));
            addEntry(
                entries, foldedPaths, key,
                {entry.path(), 0, directory ? 0 : regularFileSize(entry.path()),
                 0, modificationTime(entry.path()), directory, false});
        }
        iterator.increment(error);
        if (error) {
            throw std::runtime_error("Failed to enumerate asset group " +
                                     group + ": " + error.message());
        }
    }
}

void loadPackage(const std::filesystem::path& packagePath,
                 std::unordered_map<std::string, StoreEntry>& entries,
                 std::unordered_map<std::string, std::string>& foldedPaths) {
    detail::LdPakArchive archive(packagePath);
    const std::string& group = archive.group();
    static_cast<void>(makeAssetPath(group, "validation"));
    const std::string groupKey = "/Game/Assets/" + group;
    addEntry(entries, foldedPaths, groupKey,
             {archive.path(), 0, 0, 0, archive.modificationTime(), true, true});
    for (const detail::LdPakEntry& archiveEntry : archive.entries()) {
        addEntry(entries, foldedPaths, makeAssetPath(group, archiveEntry.path),
                 {archive.path(), archiveEntry.offset, archiveEntry.size,
                  archiveEntry.crc, archive.modificationTime(),
                  archiveEntry.directory, true});
    }
}

}  // namespace

AssetStore::AssetStore() : impl_(std::make_unique<Impl>()) {}
AssetStore::~AssetStore() = default;

void AssetStore::configure(const std::filesystem::path& runtimeRoot,
                           const AssetStoreMode mode) {
    std::error_code error;
    const std::filesystem::path normalized =
        std::filesystem::weakly_canonical(runtimeRoot, error);
    if (error || normalized.empty()) {
        throw std::invalid_argument("Invalid runtime root for AssetStore");
    }
    const std::filesystem::path assetsRoot = normalized / "Assets";
    const std::filesystem::file_status assetsStatus =
        std::filesystem::symlink_status(assetsRoot, error);
    if (error || !std::filesystem::is_directory(assetsStatus) ||
        isLinkLike(assetsRoot, assetsStatus)) {
        throw std::invalid_argument(
            "AssetStore runtime root must contain Assets");
    }

    std::vector<std::filesystem::directory_entry> groups;
    std::vector<std::filesystem::path> packages;
    std::filesystem::directory_iterator iterator(assetsRoot, error);
    if (error) {
        throw std::runtime_error("Failed to enumerate Assets: " +
                                 error.message());
    }
    const std::filesystem::directory_iterator end;
    while (iterator != end) {
        const std::filesystem::directory_entry entry = *iterator;
        const std::filesystem::file_status status = entry.symlink_status(error);
        if (error) {
            throw std::runtime_error("Failed to inspect Assets entry: " +
                                     error.message());
        }
        if (isLinkLike(entry.path(), status)) {
            throw std::runtime_error(
                "Asset symlinks are not supported: " +
                ludork::standard::pathToUtf8(entry.path()));
        }
        if (std::filesystem::is_directory(status)) {
            groups.push_back(entry);
        } else if (std::filesystem::is_regular_file(status) &&
                   asciiFold(ludork::standard::pathToUtf8(
                       entry.path().extension())) == ".ldpak") {
            packages.push_back(entry.path());
        } else if (!isIgnoredAssetMetadata(entry.path(), status)) {
            throw std::runtime_error(
                "Unsupported Assets root entry: " +
                ludork::standard::pathToUtf8(entry.path()));
        }
        iterator.increment(error);
        if (error) {
            throw std::runtime_error("Failed to enumerate Assets: " +
                                     error.message());
        }
    }
    if (mode == AssetStoreMode::Packed && !groups.empty()) {
        throw std::runtime_error(
            "Packed Assets may contain only .ldpak group files");
    }
    if (mode == AssetStoreMode::Loose && !packages.empty()) {
        throw std::runtime_error(
            "Loose Assets may contain only first-level group directories");
    }

    std::unordered_map<std::string, StoreEntry> loadedEntries;
    std::unordered_map<std::string, std::string> foldedPaths;
    const AssetStoreMode loadedMode = mode;
    if (loadedMode == AssetStoreMode::Loose) {
        std::sort(
            groups.begin(), groups.end(),
            [](const auto& left, const auto& right) {
                return ludork::standard::pathToUtf8(left.path().filename()) <
                       ludork::standard::pathToUtf8(right.path().filename());
            });
        for (const std::filesystem::directory_entry& group : groups) {
            loadLooseGroup(assetsRoot, group, loadedEntries, foldedPaths);
        }
    } else {
        std::sort(packages.begin(), packages.end());
        for (const std::filesystem::path& package : packages) {
            loadPackage(package, loadedEntries, foldedPaths);
        }
    }

    std::unique_lock lock(impl_->mutex);
    impl_->runtimeRoot = normalized;
    impl_->mode = loadedMode;
    impl_->entries = std::move(loadedEntries);
    {
        std::lock_guard validationLock(impl_->validationMutex);
        impl_->validatedEntries.clear();
    }
    impl_->configured = true;
}

void AssetStore::reset() noexcept {
    std::unique_lock lock(impl_->mutex);
    impl_->runtimeRoot.clear();
    impl_->entries.clear();
    {
        std::lock_guard validationLock(impl_->validationMutex);
        impl_->validatedEntries.clear();
    }
    impl_->mode = AssetStoreMode::Loose;
    impl_->configured = false;
}

bool AssetStore::isConfigured() const noexcept {
    std::shared_lock lock(impl_->mutex);
    return impl_->configured;
}

AssetStoreMode AssetStore::mode() const {
    std::shared_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("AssetStore is not configured");
    }
    return impl_->mode;
}

bool AssetStore::exists(const std::string& assetPath) const {
    return stat(assetPath).has_value();
}

std::optional<AssetStat> AssetStore::stat(const std::string& assetPath) const {
    const AssetPath parsed = AssetPath::parse(assetPath);
    std::shared_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("AssetStore is not configured");
    }
    if (impl_->mode == AssetStoreMode::Loose) {
        const std::optional<StoreEntry> entry =
            findLooseEntry(impl_->runtimeRoot / "Assets", parsed);
        return entry.has_value() ? std::optional<AssetStat>(
                                       AssetStat{entry->directory, entry->size,
                                                 entry->modificationTime})
                                 : std::nullopt;
    }
    const auto iterator = impl_->entries.find(assetPath);
    if (iterator == impl_->entries.end()) {
        return std::nullopt;
    }
    return AssetStat{iterator->second.directory, iterator->second.size,
                     iterator->second.modificationTime};
}

std::unique_ptr<AssetInputStream> AssetStore::open(
    const std::string& assetPath) const {
    const AssetPath parsed = AssetPath::parse(assetPath);
    std::shared_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("AssetStore is not configured");
    }
    std::optional<StoreEntry> resolved;
    if (impl_->mode == AssetStoreMode::Loose) {
        resolved = findLooseEntry(impl_->runtimeRoot / "Assets", parsed);
    } else {
        const auto iterator = impl_->entries.find(assetPath);
        if (iterator != impl_->entries.end()) {
            resolved = iterator->second;
        }
    }
    if (!resolved.has_value() || resolved->directory) {
        throw std::runtime_error("Asset file not found: " + assetPath);
    }
    const StoreEntry& entry = *resolved;
    if (entry.packed) {
        std::lock_guard validationLock(impl_->validationMutex);
        if (!impl_->validatedEntries.contains(assetPath)) {
            if (detail::calculateLdPakDataCrc(entry.source, entry.offset,
                                              entry.size) != entry.crc) {
                throw std::runtime_error("Asset package data CRC mismatch: " +
                                         assetPath);
            }
            impl_->validatedEntries.insert(assetPath);
        }
    }
    return std::unique_ptr<AssetInputStream>(
        new AssetInputStream(entry.source, entry.offset, entry.size));
}

std::vector<std::uint8_t> AssetStore::readAll(
    const std::string& assetPath) const {
    std::unique_ptr<AssetInputStream> stream = open(assetPath);
    const std::optional<std::size_t> size = stream->getSize();
    if (!size.has_value()) {
        throw std::runtime_error("Failed to inspect asset stream: " +
                                 assetPath);
    }
    std::vector<std::uint8_t> result(*size);
    std::size_t position = 0;
    while (position < result.size()) {
        const std::optional<std::size_t> count =
            stream->read(result.data() + position, result.size() - position);
        if (!count.has_value() || *count == 0) {
            throw std::runtime_error("Failed to read complete asset: " +
                                     assetPath);
        }
        position += *count;
    }
    return result;
}

AssetStore& assetStore() {
    static AssetStore store;
    return store;
}

}  // namespace ludork::runtime
