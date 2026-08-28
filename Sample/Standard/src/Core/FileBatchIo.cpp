#include "FileBatchInternal.hpp"

#include <DataFile.hpp>
#include <Utf8Path.hpp>

#include <array>
#include <cerrno>
#include <fstream>

namespace ludork::standard {

namespace {

constexpr std::size_t READ_CHUNK_SIZE = 64 * 1024;

FileBatchError ioError(std::string operation, const ManifestEntry& entry,
                       std::error_code error) {
    if (!error) {
        error = std::make_error_code(std::errc::io_error);
    }
    return {
        std::move(operation), entry.category,  pathToUtf8(entry.path),
        error.value(),        error.message(),
    };
}

FileBatchError readError(const ManifestEntry& entry,
                         const std::string& message) {
    return {
        "read", entry.category, pathToUtf8(entry.path), 0, message,
    };
}

FileBatchError parseError(const ManifestEntry& entry,
                          const std::string& message) {
    return {
        "parse", entry.category, pathToUtf8(entry.path), 0, message,
    };
}

}  // namespace

ReadResult readFile(const std::shared_ptr<FileBatchJob>& job,
                    const ManifestEntry& entry) {
    ReadResult result;
    if (entry.jsonData) {
        if (job->cancellationRequested.load(std::memory_order_relaxed)) {
            result.cancelled = true;
            return result;
        }
        try {
            std::string content = readJsonText(entry.path);
            if (job->cancellationRequested.load(std::memory_order_relaxed)) {
                result.cancelled = true;
                return result;
            }
            FileBatchParsedJson parsedJson;
            std::size_t contentBytes = 0;
            if (entry.parseJson) {
                contentBytes = content.size();
                try {
                    parsedJson = job->jsonParser(content);
                } catch (const std::exception& exception) {
                    result.error = parseError(entry, exception.what());
                    return result;
                }
                if (!parsedJson) {
                    result.error =
                        parseError(entry, "JSON parser returned no data");
                    return result;
                }
                std::string{}.swap(content);
            }
            if (job->cancellationRequested.load(std::memory_order_relaxed)) {
                result.cancelled = true;
                return result;
            }
            result.item = FileBatchItem{
                entry.index,        entry.category,      entry.relativePath,
                std::move(content), entry.encryptedData, std::move(parsedJson),
                contentBytes,
            };
        } catch (const std::exception& exception) {
            result.error = readError(entry, exception.what());
        }
        return result;
    }

    errno = 0;
    std::ifstream input(entry.path, std::ios::binary);
    if (!input) {
        result.error = ioError(
            "open", entry,
            errno == 0 ? std::error_code{}
                       : std::error_code(errno, std::generic_category()));
        return result;
    }

    std::string content;
    if (entry.fileSize <= content.max_size()) {
        content.reserve(static_cast<std::size_t>(entry.fileSize));
    }
    std::array<char, READ_CHUNK_SIZE> buffer{};
    while (input) {
        if (job->cancellationRequested.load(std::memory_order_relaxed)) {
            result.cancelled = true;
            return result;
        }
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            content.append(buffer.data(), static_cast<std::size_t>(count));
        }
    }
    if (input.bad()) {
        result.error = ioError("read", entry, {});
        return result;
    }
    if (static_cast<std::uintmax_t>(content.size()) != entry.fileSize) {
        result.error = ioError("read", entry, {});
        return result;
    }
    result.item = FileBatchItem{
        entry.index,        entry.category,      entry.relativePath,
        std::move(content), entry.encryptedData,
    };
    return result;
}

}  // namespace ludork::standard
