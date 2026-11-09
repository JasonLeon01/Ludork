#include <Runtime/AssetStore.hpp>
#include <LudorkGenerated/ResourceFileConstants.hpp>
#include <Runtime/AssetInputStream.hpp>

#include "AssetStoreImpl.hpp"
#include "LdPakArchive.hpp"
#include "ResourceFileInfo.hpp"
#include "ResourceStorePaths.hpp"
#include <Runtime/AssetPath.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ludork::runtime {
namespace {

bool isIgnoredAssetMetadata(const std::filesystem::path& path,
                            const std::filesystem::file_status& status) {
    return std::filesystem::is_regular_file(status) &&
           path.filename() == ".DS_Store";
}

void addEntry(
    std::unordered_map<std::string, asset_store_impl::StoreEntry>& entries,
    std::unordered_map<std::string, std::string>& foldedPaths,
    const std::string& key, asset_store_impl::StoreEntry entry) {
    if (!entries.emplace(key, std::move(entry)).second) {
        throw std::runtime_error("Duplicate asset path: " + key);
    }
    const std::string folded = ludork::runtime::detail::asciiFold(key);
    const auto [iterator, inserted] = foldedPaths.emplace(folded, key);
    if (!inserted && iterator->second != key) {
        throw std::runtime_error("Asset paths differ only by case: " +
                                 iterator->second + " and " + key);
    }
}

std::optional<asset_store_impl::StoreEntry> findLooseEntry(
    const std::filesystem::path& root, const AssetPath& assetPath) {
    std::filesystem::path relative =
        std::filesystem::path(ludork::generated::resources::AssetGroup) /
        ludork::standard::pathFromUtf8(assetPath.group);
    if (!assetPath.relativePath.empty()) {
        relative /= ludork::standard::pathFromUtf8(assetPath.relativePath);
    }
    const auto entry = detail::findLooseResource(root, relative);
    if (!entry) {
        return std::nullopt;
    }
    const bool directory = entry->is_directory();
    return asset_store_impl::StoreEntry{
        entry->path(),
        directory ? 0
                  : detail::resourceFileSize(entry->path(),
                                             "Failed to read asset file size: ",
                                             "Asset file is too large"),
        detail::resourceModificationTime(
            entry->path(), "Failed to read asset modification time: "),
        directory,
        {},
        {}};
}

std::vector<std::uint8_t> readStream(AssetInputStream& stream,
                                     const std::string& assetPath) {
    const std::optional<std::size_t> size = stream.getSize();
    if (!size.has_value()) {
        throw std::runtime_error("Failed to inspect asset stream: " +
                                 assetPath);
    }
    std::vector<std::uint8_t> result(*size);
    std::size_t position = 0;
    while (position < result.size()) {
        const std::optional<std::size_t> count =
            stream.read(result.data() + position, result.size() - position);
        if (!count.has_value() || *count == 0) {
            throw std::runtime_error("Failed to read complete asset: " +
                                     assetPath);
        }
        position += *count;
    }
    return result;
}

void loadLooseTree(
    const std::filesystem::path& assetsRoot,
    std::unordered_map<std::string, asset_store_impl::StoreEntry>& entries,
    std::unordered_map<std::string, std::string>& foldedPaths) {
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        assetsRoot, std::filesystem::directory_options::none, error);
    if (error) {
        throw std::runtime_error("Failed to enumerate Assets: " +
                                 error.message());
    }
    const std::filesystem::recursive_directory_iterator end;
    while (iterator != end) {
        const std::filesystem::directory_entry entry = *iterator;
        const std::filesystem::file_status status = entry.symlink_status(error);
        if (error) {
            throw std::runtime_error("Failed to inspect loose asset: " +
                                     error.message());
        }
        if (ludork::runtime::detail::isLinkLike(
                entry.path(), status,
                "Failed to inspect asset filesystem entry")) {
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
            const std::string key =
                ludork::generated::resources::AssetPathPrefix + relativeText;
            static_cast<void>(AssetPath::parse(key));
            addEntry(
                entries, foldedPaths, key,
                {entry.path(),
                 directory
                     ? 0
                     : ludork::runtime::detail::resourceFileSize(
                           entry.path(), "Failed to read asset file size: ",
                           "Asset file is too large"),
                 ludork::runtime::detail::resourceModificationTime(
                     entry.path(), "Failed to read asset modification time: "),
                 directory,
                 {},
                 {}});
        }
        iterator.increment(error);
        if (error) {
            throw std::runtime_error("Failed to enumerate Assets: " +
                                     error.message());
        }
    }
}

void loadPackage(
    const std::filesystem::path& packagePath,
    std::unordered_map<std::string, asset_store_impl::StoreEntry>& entries,
    std::unordered_map<std::string, std::string>& foldedPaths) {
    auto archive = std::make_shared<detail::LdPakArchive>(packagePath);
    if (archive->group() != ludork::generated::resources::AssetGroup) {
        throw std::runtime_error("Assets.ldpak must use the Assets group");
    }
    for (const detail::LdPakEntry& archiveEntry : archive->entries()) {
        const std::string key =
            ludork::generated::resources::AssetPathPrefix + archiveEntry.path;
        static_cast<void>(AssetPath::parse(key));
        addEntry(
            entries, foldedPaths, key,
            {archive->path(), archiveEntry.size, archive->modificationTime(),
             archiveEntry.directory, archive, archiveEntry.path});
    }
}

}  // namespace

AssetStore::AssetStore() : impl_(std::make_unique<Impl>()) {}
AssetStore::~AssetStore() = default;

void AssetStore::configure(const std::filesystem::path& runtimeRoot,
                           const AssetStoreMode mode) {
    const std::filesystem::path normalized = detail::resourceStoreRoot(
        runtimeRoot, ludork::generated::resources::AssetGroup,
        mode == AssetStoreMode::Packed);
    std::unordered_map<std::string, asset_store_impl::StoreEntry> loadedEntries;
    std::unordered_map<std::string, std::string> foldedPaths;
    if (mode == AssetStoreMode::Loose) {
        loadLooseTree(normalized / ludork::generated::resources::AssetGroup,
                      loadedEntries, foldedPaths);
    } else {
        loadPackage(normalized /
                        (std::string(ludork::generated::resources::AssetGroup) +
                         ludork::generated::resources::PackageExtension),
                    loadedEntries, foldedPaths);
    }

    std::unique_lock lock(impl_->mutex);
    impl_->runtimeRoot = normalized;
    impl_->userDataRoot.clear();
    impl_->mode = mode;
    impl_->entries = std::move(loadedEntries);
    impl_->configured = true;
}

void AssetStore::setUserDataRoot(const std::filesystem::path& root) {
    const std::filesystem::path normalized = detail::resourceUserDataRoot(root);
    std::unique_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("AssetStore is not configured");
    }
    impl_->userDataRoot = normalized;
}

std::optional<std::pair<std::string, asset_store_impl::StoreEntry>>
AssetStore::Impl::resolve(
    const std::vector<std::string>& candidatePaths) const {
    if (!configured) {
        throw std::logic_error("AssetStore is not configured");
    }
    std::vector<AssetPath> parsed;
    parsed.reserve(candidatePaths.size());
    for (const std::string& path : candidatePaths) {
        parsed.push_back(AssetPath::parse(path));
    }
    for (const bool user : {true, false}) {
        for (const AssetPath& path : parsed) {
            if (user || mode == AssetStoreMode::Loose) {
                if (auto entry = findLooseEntry(
                        user ? userDataRoot : runtimeRoot, path)) {
                    return std::make_pair(path.value, std::move(*entry));
                }
            } else {
                const auto entry = entries.find(path.value);
                if (entry != entries.end()) {
                    return std::make_pair(path.value, entry->second);
                }
            }
        }
    }
    return std::nullopt;
}

void AssetStore::reset() noexcept {
    std::unique_lock lock(impl_->mutex);
    impl_->runtimeRoot.clear();
    impl_->userDataRoot.clear();
    impl_->entries.clear();
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

std::optional<AssetStore::AssetStat> AssetStore::stat(
    const std::string& assetPath) const {
    std::shared_lock lock(impl_->mutex);
    const auto resolved = impl_->resolve({assetPath});
    if (!resolved) {
        return std::nullopt;
    }
    const asset_store_impl::StoreEntry& entry = resolved->second;
    return AssetStat{entry.directory, entry.size, entry.modificationTime};
}

std::unique_ptr<AssetInputStream> AssetStore::open(
    const std::string& assetPath) const {
    std::shared_lock lock(impl_->mutex);
    const auto resolved = impl_->resolve({assetPath});
    if (!resolved || resolved->second.directory) {
        throw std::runtime_error("Asset file not found: " + assetPath);
    }
    const asset_store_impl::StoreEntry& entry = resolved->second;
    if (entry.archive) {
        return std::unique_ptr<AssetInputStream>(
            new AssetInputStream(entry.archive, entry.archivePath));
    }
    return std::unique_ptr<AssetInputStream>(
        new AssetInputStream(entry.source, 0, entry.size));
}

std::vector<std::uint8_t> AssetStore::readAll(
    const std::string& assetPath) const {
    std::unique_ptr<AssetInputStream> stream = open(assetPath);
    return readStream(*stream, assetPath);
}

std::optional<AssetStore::AssetContents> AssetStore::readFirst(
    const std::vector<std::string>& candidatePaths) const {
    std::shared_lock lock(impl_->mutex);
    const auto resolved = impl_->resolve(candidatePaths);
    if (!resolved) {
        return std::nullopt;
    }
    const auto& [path, entry] = *resolved;
    if (entry.directory) {
        throw std::runtime_error("Asset is not a file: " + path);
    }
    std::unique_ptr<AssetInputStream> stream =
        entry.archive ? std::unique_ptr<AssetInputStream>(new AssetInputStream(
                            entry.archive, entry.archivePath))
                      : std::unique_ptr<AssetInputStream>(
                            new AssetInputStream(entry.source, 0, entry.size));
    lock.unlock();
    return AssetContents{path, readStream(*stream, path)};
}

AssetStore& assetStore() {
    static AssetStore store;
    return store;
}

}  // namespace ludork::runtime
