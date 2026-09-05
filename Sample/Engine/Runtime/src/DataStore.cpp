#include <Runtime/DataStore.hpp>

#include "LdPakArchive.hpp"
#include <ReadOnlyFileProvider.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

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
        throw std::runtime_error("Failed to inspect Data filesystem entry");
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    static_cast<void>(path);
    return false;
#endif
}

bool isIgnoredMetadata(const std::filesystem::path& path,
                       const std::filesystem::file_status& status) {
    return std::filesystem::is_regular_file(status) &&
           path.filename() == ".DS_Store";
}

double modificationTime(const std::filesystem::path& path) {
    std::error_code error;
    const std::filesystem::file_time_type writeTime =
        std::filesystem::last_write_time(path, error);
    if (error) {
        throw std::runtime_error("Failed to read Data modification time: " +
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
        throw std::runtime_error("Failed to read Data file size: " +
                                 error.message());
    }
    if (size > std::numeric_limits<std::uint64_t>::max()) {
        throw std::runtime_error("Data file is too large");
    }
    return static_cast<std::uint64_t>(size);
}

void validateGroup(const std::string& group) {
    if (group.empty() || group == "." || group == ".." ||
        group.find('/') != std::string::npos ||
        group.find('\\') != std::string::npos ||
        group.find('\0') != std::string::npos ||
        asciiFold(group).ends_with(".ldpak")) {
        throw std::runtime_error("Invalid Data group: " + group);
    }
}

std::vector<std::uint8_t> readPhysicalFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open Data file: " +
                                 ludork::standard::pathToUtf8(path));
    }
    std::vector<std::uint8_t> result{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Failed to read Data file: " +
                                 ludork::standard::pathToUtf8(path));
    }
    return result;
}

}  // namespace

namespace ludork::runtime {

namespace {

struct StoreEntry {
    std::filesystem::path source;
    std::shared_ptr<detail::LdPakArchive> archive;
    std::string archivePath;
    DataStat stat;
};

void addEntry(std::unordered_map<std::string, StoreEntry>& entries,
              std::unordered_map<std::string, std::string>& foldedPaths,
              const std::string& key, StoreEntry entry) {
    if (!entries.emplace(key, std::move(entry)).second) {
        throw std::runtime_error("Duplicate Data path: " + key);
    }
    const std::string folded = asciiFold(key);
    const auto [iterator, inserted] = foldedPaths.emplace(folded, key);
    if (!inserted && iterator->second != key) {
        throw std::runtime_error("Data paths differ only by case: " +
                                 iterator->second + " and " + key);
    }
}

void loadLooseGroup(const std::filesystem::path& dataRoot,
                    const std::filesystem::directory_entry& groupEntry,
                    std::unordered_map<std::string, StoreEntry>& entries,
                    std::unordered_map<std::string, std::string>& foldedPaths) {
    const std::string group =
        ludork::standard::pathToUtf8(groupEntry.path().filename());
    validateGroup(group);
    addEntry(entries, foldedPaths, "Data/" + group,
             {groupEntry.path(),
              nullptr,
              {},
              {true, 0, modificationTime(groupEntry.path())}});

    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        groupEntry.path(), std::filesystem::directory_options::none, error);
    if (error) {
        throw std::runtime_error("Failed to enumerate Data group " + group +
                                 ": " + error.message());
    }
    const std::filesystem::recursive_directory_iterator end;
    while (iterator != end) {
        const std::filesystem::directory_entry entry = *iterator;
        const std::filesystem::file_status status = entry.symlink_status(error);
        if (error) {
            throw std::runtime_error("Failed to inspect loose Data: " +
                                     error.message());
        }
        if (isLinkLike(entry.path(), status)) {
            throw std::runtime_error(
                "Data symlinks are not supported: " +
                ludork::standard::pathToUtf8(entry.path()));
        }
        if (!isIgnoredMetadata(entry.path(), status)) {
            const bool directory = std::filesystem::is_directory(status);
            if (!directory && !std::filesystem::is_regular_file(status)) {
                throw std::runtime_error(
                    "Unsupported loose Data entry: " +
                    ludork::standard::pathToUtf8(entry.path()));
            }
            const std::filesystem::path relative =
                entry.path().lexically_relative(dataRoot);
            const std::string key =
                "Data/" + ludork::standard::pathToGenericUtf8(relative);
            addEntry(entries, foldedPaths, key,
                     {entry.path(),
                      nullptr,
                      {},
                      {directory, directory ? 0 : regularFileSize(entry.path()),
                       modificationTime(entry.path())}});
        }
        iterator.increment(error);
        if (error) {
            throw std::runtime_error("Failed to enumerate Data group " + group +
                                     ": " + error.message());
        }
    }
}

void loadPackage(const std::filesystem::path& packagePath,
                 std::unordered_map<std::string, StoreEntry>& entries,
                 std::unordered_map<std::string, std::string>& foldedPaths) {
    std::shared_ptr<detail::LdPakArchive> archive =
        std::make_shared<detail::LdPakArchive>(packagePath);
    const std::string& group = archive->group();
    validateGroup(group);
    addEntry(
        entries, foldedPaths, "Data/" + group,
        {archive->path(), archive, {}, {true, 0, archive->modificationTime()}});
    for (const detail::LdPakEntry& archiveEntry : archive->entries()) {
        addEntry(entries, foldedPaths,
                 "Data/" + group + "/" + archiveEntry.path,
                 {archive->path(),
                  archive,
                  archiveEntry.path,
                  {archiveEntry.directory, archiveEntry.size,
                   archive->modificationTime()}});
    }
}

std::unordered_map<std::string, std::vector<std::filesystem::path>>
buildDirectoryEntries(
    const std::unordered_map<std::string, StoreEntry>& entries) {
    std::unordered_map<std::string, std::vector<std::filesystem::path>> result;
    for (const auto& [key, entry] : entries) {
        static_cast<void>(entry);
        if (key == "Data") {
            continue;
        }
        const std::size_t separator = key.rfind('/');
        if (separator == std::string::npos) {
            throw std::runtime_error("Invalid indexed Data path: " + key);
        }
        result[key.substr(0, separator)].push_back(
            ludork::standard::pathFromUtf8(key.substr(separator + 1)));
    }
    for (auto& [directory, children] : result) {
        static_cast<void>(directory);
        std::sort(children.begin(), children.end());
    }
    return result;
}

std::optional<std::string> normalizeDataPath(
    const std::filesystem::path& runtimeRoot,
    const std::filesystem::path& path) {
    if (path.empty()) {
        return std::nullopt;
    }
    std::filesystem::path relative;
    if (path.is_absolute()) {
        relative = path.lexically_normal().lexically_relative(runtimeRoot);
    } else {
        relative = path.lexically_normal();
    }
    if (relative.empty() || relative.is_absolute()) {
        return std::nullopt;
    }
    for (const std::filesystem::path& part : relative) {
        if (part == "..") {
            return std::nullopt;
        }
    }
    const std::string key = ludork::standard::pathToGenericUtf8(relative);
    if (key != "Data" && !key.starts_with("Data/")) {
        return std::nullopt;
    }
    return key;
}

}  // namespace

struct DataStore::Impl {
    mutable std::shared_mutex mutex;
    std::filesystem::path runtimeRoot;
    DataStoreMode mode = DataStoreMode::Loose;
    bool configured = false;
    std::unordered_map<std::string, StoreEntry> entries;
    std::unordered_map<std::string, std::vector<std::filesystem::path>>
        directoryEntries;
};

DataStore::DataStore() : impl_(std::make_unique<Impl>()) {}
DataStore::~DataStore() = default;

void DataStore::configure(const std::filesystem::path& runtimeRoot,
                          const DataStoreMode mode) {
    std::error_code error;
    const std::filesystem::path normalized =
        std::filesystem::weakly_canonical(runtimeRoot, error);
    if (error || normalized.empty()) {
        throw std::invalid_argument("Invalid runtime root for DataStore");
    }
    const std::filesystem::path dataRoot = normalized / "Data";
    const std::filesystem::file_status dataStatus =
        std::filesystem::symlink_status(dataRoot, error);
    if (error || !std::filesystem::is_directory(dataStatus) ||
        isLinkLike(dataRoot, dataStatus)) {
        throw std::invalid_argument("DataStore runtime root must contain Data");
    }

    std::vector<std::filesystem::directory_entry> groups;
    std::vector<std::filesystem::path> packages;
    std::filesystem::directory_iterator iterator(dataRoot, error);
    if (error) {
        throw std::runtime_error("Failed to enumerate Data: " +
                                 error.message());
    }
    const std::filesystem::directory_iterator end;
    while (iterator != end) {
        const std::filesystem::directory_entry entry = *iterator;
        const std::filesystem::file_status status = entry.symlink_status(error);
        if (error) {
            throw std::runtime_error("Failed to inspect Data entry: " +
                                     error.message());
        }
        if (isLinkLike(entry.path(), status)) {
            throw std::runtime_error(
                "Data symlinks are not supported: " +
                ludork::standard::pathToUtf8(entry.path()));
        }
        if (std::filesystem::is_directory(status)) {
            groups.push_back(entry);
        } else if (std::filesystem::is_regular_file(status) &&
                   asciiFold(ludork::standard::pathToUtf8(
                       entry.path().extension())) == ".ldpak") {
            packages.push_back(entry.path());
        } else if (!isIgnoredMetadata(entry.path(), status)) {
            throw std::runtime_error(
                "Unsupported Data root entry: " +
                ludork::standard::pathToUtf8(entry.path()));
        }
        iterator.increment(error);
        if (error) {
            throw std::runtime_error("Failed to enumerate Data: " +
                                     error.message());
        }
    }
    if (mode == DataStoreMode::Packed && !groups.empty()) {
        throw std::runtime_error(
            "Packed Data may contain only .ldpak group files");
    }
    if (mode == DataStoreMode::Loose && !packages.empty()) {
        throw std::runtime_error(
            "Loose Data may contain only first-level group directories");
    }

    std::unordered_map<std::string, StoreEntry> loadedEntries;
    std::unordered_map<std::string, std::string> foldedPaths;
    addEntry(loadedEntries, foldedPaths, "Data",
             {dataRoot, nullptr, {}, {true, 0, modificationTime(dataRoot)}});
    const DataStoreMode loadedMode = mode;
    if (loadedMode == DataStoreMode::Loose) {
        std::sort(
            groups.begin(), groups.end(),
            [](const auto& left, const auto& right) {
                return ludork::standard::pathToUtf8(left.path().filename()) <
                       ludork::standard::pathToUtf8(right.path().filename());
            });
        for (const std::filesystem::directory_entry& group : groups) {
            loadLooseGroup(dataRoot, group, loadedEntries, foldedPaths);
        }
    } else {
        std::sort(packages.begin(), packages.end());
        for (const std::filesystem::path& package : packages) {
            loadPackage(package, loadedEntries, foldedPaths);
        }
    }
    auto loadedDirectoryEntries = buildDirectoryEntries(loadedEntries);

    {
        std::unique_lock lock(impl_->mutex);
        impl_->runtimeRoot = normalized;
        impl_->mode = loadedMode;
        impl_->entries = std::move(loadedEntries);
        impl_->directoryEntries = std::move(loadedDirectoryEntries);
        impl_->configured = true;
    }

    if (loadedMode == DataStoreMode::Packed) {
        ludork::standard::configureReadOnlyFileProvider({
            [this](const std::filesystem::path& path) {
                ludork::standard::ReadOnlyFileStatus result;
                if (!handles(path)) {
                    return result;
                }
                result.handled = true;
                const std::optional<DataStat> value = stat(path);
                if (!value.has_value()) {
                    return result;
                }
                result.type =
                    value->directory
                        ? ludork::standard::ReadOnlyFileType::Directory
                        : ludork::standard::ReadOnlyFileType::Regular;
                result.size = value->size;
                result.modificationTime = value->modificationTime;
                return result;
            },
            [this](const std::filesystem::path& path) {
                return listDirectory(path);
            },
            [this](const std::filesystem::path& path) {
                return readAll(path);
            },
        });
    } else {
        ludork::standard::clearReadOnlyFileProvider();
    }
}

void DataStore::reset() noexcept {
    ludork::standard::clearReadOnlyFileProvider();
    std::unique_lock lock(impl_->mutex);
    impl_->runtimeRoot.clear();
    impl_->entries.clear();
    impl_->directoryEntries.clear();
    impl_->mode = DataStoreMode::Loose;
    impl_->configured = false;
}

bool DataStore::isConfigured() const noexcept {
    std::shared_lock lock(impl_->mutex);
    return impl_->configured;
}

DataStoreMode DataStore::mode() const {
    std::shared_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("DataStore is not configured");
    }
    return impl_->mode;
}

bool DataStore::handles(const std::filesystem::path& path) const {
    std::shared_lock lock(impl_->mutex);
    return impl_->configured &&
           normalizeDataPath(impl_->runtimeRoot, path).has_value();
}

std::optional<DataStat> DataStore::stat(
    const std::filesystem::path& path) const {
    std::shared_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("DataStore is not configured");
    }
    const std::optional<std::string> key =
        normalizeDataPath(impl_->runtimeRoot, path);
    if (!key.has_value()) {
        throw std::invalid_argument("Path is outside Data");
    }
    const auto iterator = impl_->entries.find(*key);
    return iterator == impl_->entries.end()
               ? std::nullopt
               : std::optional<DataStat>(iterator->second.stat);
}

std::vector<std::filesystem::path> DataStore::listDirectory(
    const std::filesystem::path& path) const {
    std::shared_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("DataStore is not configured");
    }
    const std::optional<std::string> key =
        normalizeDataPath(impl_->runtimeRoot, path);
    if (!key.has_value()) {
        throw std::invalid_argument("Path is outside Data");
    }
    const auto entry = impl_->entries.find(*key);
    if (entry == impl_->entries.end() || !entry->second.stat.directory) {
        throw std::runtime_error("Data directory not found: " + *key);
    }
    const auto children = impl_->directoryEntries.find(*key);
    return children == impl_->directoryEntries.end()
               ? std::vector<std::filesystem::path>{}
               : children->second;
}

std::vector<std::uint8_t> DataStore::readAll(
    const std::filesystem::path& path) const {
    std::shared_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("DataStore is not configured");
    }
    const std::optional<std::string> key =
        normalizeDataPath(impl_->runtimeRoot, path);
    if (!key.has_value()) {
        throw std::invalid_argument("Path is outside Data");
    }
    const auto iterator = impl_->entries.find(*key);
    if (iterator == impl_->entries.end() || iterator->second.stat.directory) {
        throw std::runtime_error("Data file not found: " + *key);
    }
    const StoreEntry& entry = iterator->second;
    return entry.archive ? entry.archive->readAll(entry.archivePath)
                         : readPhysicalFile(entry.source);
}

DataStore& dataStore() {
    static DataStore store;
    return store;
}

}  // namespace ludork::runtime
