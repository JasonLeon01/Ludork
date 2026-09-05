#pragma once

#include "FileBatch.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ludork::standard {

class FileBatchJob {
public:
    explicit FileBatchJob(std::vector<FileBatchSpec> batchSpecs,
                          std::size_t capacity,
                          FileBatchJsonParser batchJsonParser,
                          FileBatchJsonBegin batchJsonBegin,
                          FileBatchJsonStep batchJsonStep,
                          FileBatchJsonClear batchJsonClear)
        : specs(std::move(batchSpecs)),
          resultCapacity(capacity),
          jsonParser(std::move(batchJsonParser)),
          jsonBegin(std::move(batchJsonBegin)),
          jsonStep(std::move(batchJsonStep)),
          jsonClear(std::move(batchJsonClear)) {}

    std::mutex mutex;
    std::condition_variable resultSpace;
    std::vector<FileBatchSpec> specs;
    std::map<std::size_t, FileBatchItem> results;
    std::optional<FileBatchError> error;
    std::atomic_bool cancellationRequested = false;
    FileBatchState state = FileBatchState::Scanning;
    std::size_t resultCapacity = 0;
    std::size_t total = 0;
    std::size_t completed = 0;
    std::size_t delivered = 0;
    std::size_t nextDeliveryIndex = 1;
    std::size_t pendingWork = 0;
    FileBatchJsonParser jsonParser;
    FileBatchJsonBegin jsonBegin;
    FileBatchJsonStep jsonStep;
    FileBatchJsonClear jsonClear;
};

class FileBatchJsonConversionState {
public:
    std::mutex mutex;
    std::weak_ptr<FileBatchJob> job;
    FileBatchJsonConversion conversion;
    FileBatchJsonStep step;
    FileBatchJsonClear clear;
    bool completed = false;
    bool cleared = false;
};

struct ManifestEntry {
    std::size_t index = 0;
    std::string category;
    std::filesystem::path path;
    std::string relativePath;
    std::uintmax_t fileSize = 0;
    bool jsonData = false;
    bool parseJson = false;
    bool encryptedData = false;
};

enum class WorkKind {
    Scan,
    Read,
};

struct WorkItem {
    WorkKind kind = WorkKind::Scan;
    std::shared_ptr<FileBatchJob> job;
    ManifestEntry entry;
};

struct ScanResult {
    std::vector<ManifestEntry> entries;
    std::optional<FileBatchError> error;
    bool cancelled = false;
};

struct ReadResult {
    std::optional<FileBatchItem> item;
    std::optional<FileBatchError> error;
    bool cancelled = false;
};

ReadResult readFile(const std::shared_ptr<FileBatchJob>& job,
                    const ManifestEntry& entry);

}  // namespace ludork::standard
