#include "FileBatchInternal.hpp"

#include <DataFile.hpp>
#include <ReadOnlyFileProvider.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <exception>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ludork::standard {

namespace {

FileBatchError filesystemError(std::string operation, const FileBatchSpec& spec,
                               const std::filesystem::path& path,
                               const std::error_code& error) {
    return {
        std::move(operation), spec.category,   pathToUtf8(path),
        error.value(),        error.message(),
    };
}

FileBatchError providerError(std::string operation, const FileBatchSpec& spec,
                             const std::filesystem::path& path,
                             std::string message) {
    return {
        std::move(operation), spec.category, pathToUtf8(path), 0,
        std::move(message),
    };
}

bool suffixMatches(std::string_view path, std::string_view suffix) {
    return suffix.empty() ||
           (path.size() >= suffix.size() &&
            path.substr(path.size() - suffix.size()) == suffix);
}

bool jsonSuffix(std::string_view suffix) {
    return suffixMatches(suffix, ".json");
}

template <typename Iterator>
bool appendDirectoryEntries(const FileBatchSpec& spec,
                            const std::shared_ptr<FileBatchJob>& job,
                            std::vector<ManifestEntry>& entries,
                            FileBatchError& error) {
    std::error_code iteratorError;
    Iterator iterator(spec.root, std::filesystem::directory_options::none,
                      iteratorError);
    const Iterator end;
    if (iteratorError) {
        error = filesystemError("scan", spec, spec.root, iteratorError);
        return false;
    }
    while (iterator != end) {
        if (job->cancellationRequested.load(std::memory_order_relaxed)) {
            return false;
        }
        std::error_code typeError;
        const bool regular = iterator->is_regular_file(typeError);
        if (typeError) {
            error = filesystemError("scan", spec, iterator->path(), typeError);
            return false;
        }
        if (regular) {
            const std::filesystem::path relative =
                iterator->path().lexically_relative(spec.root);
            const std::string storedRelativePath = pathToGenericUtf8(relative);
            const bool encryptedData = iterator->path().extension() == ".ldc";
            const bool includeEncryptedData =
                encryptedData && jsonSuffix(spec.suffix);
            const std::string relativePath =
                includeEncryptedData
                    ? pathToGenericUtf8(logicalJsonDataPath(relative))
                    : storedRelativePath;
            if (suffixMatches(relativePath, spec.suffix) &&
                (spec.excludeSuffix.empty() ||
                 !suffixMatches(relativePath, spec.excludeSuffix)) &&
                (!encryptedData || includeEncryptedData)) {
                std::error_code sizeError;
                const std::uintmax_t fileSize = iterator->file_size(sizeError);
                if (sizeError) {
                    error = filesystemError("scan", spec, iterator->path(),
                                            sizeError);
                    return false;
                }
                entries.push_back({
                    0,
                    spec.category,
                    iterator->path(),
                    relativePath,
                    fileSize,
                    jsonSuffix(relativePath),
                    spec.parseJson,
                    encryptedData,
                });
            }
        }
        iterator.increment(iteratorError);
        if (iteratorError) {
            error = filesystemError("scan", spec, spec.root, iteratorError);
            return false;
        }
    }
    return true;
}

bool appendReadOnlyDirectoryEntries(const FileBatchSpec& spec,
                                    const std::shared_ptr<FileBatchJob>& job,
                                    std::vector<ManifestEntry>& entries,
                                    FileBatchError& error) {
    std::vector<std::filesystem::path> pending{spec.root};
    while (!pending.empty()) {
        const std::filesystem::path directory = std::move(pending.back());
        pending.pop_back();
        std::vector<std::filesystem::path> children;
        try {
            children = readOnlyDirectoryEntries(directory);
        } catch (const std::exception& exception) {
            error = providerError("scan", spec, directory, exception.what());
            return false;
        }
        for (const std::filesystem::path& childName : children) {
            if (job->cancellationRequested.load(std::memory_order_relaxed)) {
                return false;
            }
            const std::filesystem::path child = directory / childName;
            ReadOnlyFileStatus status;
            try {
                status = readOnlyFileStatus(child);
            } catch (const std::exception& exception) {
                error = providerError("scan", spec, child, exception.what());
                return false;
            }
            if (!status.handled || status.type == ReadOnlyFileType::Missing) {
                error = providerError(
                    "scan", spec, child,
                    "Read-only file provider returned an invalid entry");
                return false;
            }
            if (status.type == ReadOnlyFileType::Directory) {
                if (spec.recursive) {
                    pending.push_back(child);
                }
                continue;
            }
            const std::filesystem::path relative =
                child.lexically_relative(spec.root);
            const std::string storedRelativePath = pathToGenericUtf8(relative);
            const bool encryptedData = child.extension() == ".ldc";
            const bool includeEncryptedData =
                encryptedData && jsonSuffix(spec.suffix);
            const std::string relativePath =
                includeEncryptedData
                    ? pathToGenericUtf8(logicalJsonDataPath(relative))
                    : storedRelativePath;
            if (suffixMatches(relativePath, spec.suffix) &&
                (spec.excludeSuffix.empty() ||
                 !suffixMatches(relativePath, spec.excludeSuffix)) &&
                (!encryptedData || includeEncryptedData)) {
                entries.push_back({
                    0,
                    spec.category,
                    child,
                    relativePath,
                    status.size,
                    jsonSuffix(relativePath),
                    spec.parseJson,
                    encryptedData,
                });
            }
        }
    }
    return true;
}

}  // namespace

ScanResult scanManifest(const std::shared_ptr<FileBatchJob>& job) {
    ScanResult result;
    std::size_t nextIndex = 1;
    for (const FileBatchSpec& spec : job->specs) {
        if (job->cancellationRequested.load(std::memory_order_relaxed)) {
            result.cancelled = true;
            return result;
        }
        std::vector<ManifestEntry> specEntries;
        FileBatchError scanError;
        bool scanned = false;
        ReadOnlyFileStatus readOnlyStatus;
        try {
            readOnlyStatus = readOnlyFileStatus(spec.root);
        } catch (const std::exception& exception) {
            result.error =
                providerError("scan", spec, spec.root, exception.what());
            return result;
        }
        if (readOnlyStatus.handled) {
            if (readOnlyStatus.type == ReadOnlyFileType::Missing) {
                if (spec.required) {
                    result.error =
                        providerError("scan", spec, spec.root,
                                      std::make_error_code(
                                          std::errc::no_such_file_or_directory)
                                          .message());
                    return result;
                }
                continue;
            }
            if (readOnlyStatus.type != ReadOnlyFileType::Directory) {
                result.error = providerError(
                    "scan", spec, spec.root,
                    std::make_error_code(std::errc::not_a_directory).message());
                return result;
            }
            scanned = appendReadOnlyDirectoryEntries(spec, job, specEntries,
                                                     scanError);
        } else {
            std::error_code existsError;
            const bool exists = std::filesystem::exists(spec.root, existsError);
            if (existsError) {
                result.error =
                    filesystemError("scan", spec, spec.root, existsError);
                return result;
            }
            if (!exists) {
                if (spec.required) {
                    result.error = filesystemError(
                        "scan", spec, spec.root,
                        std::make_error_code(
                            std::errc::no_such_file_or_directory));
                }
                if (result.error.has_value()) {
                    return result;
                }
                continue;
            }
            std::error_code directoryError;
            const bool directory =
                std::filesystem::is_directory(spec.root, directoryError);
            if (directoryError) {
                result.error =
                    filesystemError("scan", spec, spec.root, directoryError);
                return result;
            }
            if (!directory) {
                result.error = filesystemError(
                    "scan", spec, spec.root,
                    std::make_error_code(std::errc::not_a_directory));
                return result;
            }
            scanned = spec.recursive
                          ? appendDirectoryEntries<
                                std::filesystem::recursive_directory_iterator>(
                                spec, job, specEntries, scanError)
                          : appendDirectoryEntries<
                                std::filesystem::directory_iterator>(
                                spec, job, specEntries, scanError);
        }
        if (!scanned) {
            if (job->cancellationRequested.load(std::memory_order_relaxed)) {
                result.cancelled = true;
            } else {
                result.error = std::move(scanError);
            }
            return result;
        }
        std::sort(specEntries.begin(), specEntries.end(),
                  [](const ManifestEntry& left, const ManifestEntry& right) {
                      if (left.relativePath != right.relativePath) {
                          return left.relativePath < right.relativePath;
                      }
                      return left.encryptedData < right.encryptedData;
                  });
        specEntries.erase(
            std::unique(
                specEntries.begin(), specEntries.end(),
                [](const ManifestEntry& left, const ManifestEntry& right) {
                    return left.relativePath == right.relativePath;
                }),
            specEntries.end());
        for (ManifestEntry& entry : specEntries) {
            entry.index = nextIndex++;
            result.entries.push_back(std::move(entry));
        }
    }
    return result;
}

}  // namespace ludork::standard
