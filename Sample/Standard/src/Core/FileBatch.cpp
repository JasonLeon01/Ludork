#include "FileBatch.hpp"

#include <DataFile.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace ludork::standard {

namespace {
constexpr std::size_t READ_CHUNK_SIZE = 64 * 1024;

}  // namespace

const char* fileBatchStateName(FileBatchState state) {
    switch (state) {
        case FileBatchState::Scanning:
            return "scanning";
        case FileBatchState::Running:
            return "running";
        case FileBatchState::Completed:
            return "completed";
        case FileBatchState::Cancelling:
            return "cancelling";
        case FileBatchState::Cancelled:
            return "cancelled";
        case FileBatchState::Failed:
            return "failed";
    }
    return "failed";
}

namespace {

bool isTerminal(FileBatchState state) {
    return state == FileBatchState::Completed ||
           state == FileBatchState::Cancelled ||
           state == FileBatchState::Failed;
}

struct ManifestEntry {
    std::size_t index = 0;
    std::string category;
    std::filesystem::path path;
    std::string relativePath;
    std::uintmax_t fileSize = 0;
    bool jsonData = false;
    bool encryptedData = false;
};

}  // namespace

class FileBatchJob {
public:
    explicit FileBatchJob(std::vector<FileBatchSpec> batchSpecs,
                          std::size_t capacity)
        : specs(std::move(batchSpecs)), resultCapacity(capacity) {}

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
};

namespace {

enum class WorkKind {
    Scan,
    Read
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

FileBatchError filesystemError(std::string operation, const FileBatchSpec& spec,
                               const std::filesystem::path& path,
                               const std::error_code& error) {
    return {
        std::move(operation), spec.category,   pathToUtf8(path),
        error.value(),        error.message(),
    };
}

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
            const std::string storedRelativePath =
                pathToGenericUtf8(relative);
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

ScanResult scanManifest(const std::shared_ptr<FileBatchJob>& job) {
    ScanResult result;
    std::size_t nextIndex = 1;
    for (const FileBatchSpec& spec : job->specs) {
        if (job->cancellationRequested.load(std::memory_order_relaxed)) {
            result.cancelled = true;
            return result;
        }
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
                    std::make_error_code(std::errc::no_such_file_or_directory));
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

        std::vector<ManifestEntry> specEntries;
        FileBatchError scanError;
        const bool scanned =
            spec.recursive
                ? appendDirectoryEntries<
                      std::filesystem::recursive_directory_iterator>(
                      spec, job, specEntries, scanError)
                : appendDirectoryEntries<std::filesystem::directory_iterator>(
                      spec, job, specEntries, scanError);
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
            result.item = FileBatchItem{
                entry.index,        entry.category,      entry.relativePath,
                std::move(content), entry.encryptedData,
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

}  // namespace

class FileBatchRuntime::Implementation {
public:
    Implementation() {
        const unsigned int detected = std::thread::hardware_concurrency();
        workerCount_ = std::min(4U, std::max(1U, detected));
    }

    ~Implementation() {
        shutdown();
    }

    std::shared_ptr<FileBatchJob> start(std::vector<FileBatchSpec> specs) {
        std::shared_ptr<FileBatchJob> job = std::make_shared<FileBatchJob>(
            std::move(specs), static_cast<std::size_t>(workerCount_) * 2);
        {
            std::lock_guard<std::mutex> queueLock(queueMutex_);
            if (stopping_.load(std::memory_order_acquire)) {
                throw std::runtime_error("async worker is shutting down");
            }
            ensureWorkersLocked();
            {
                std::lock_guard<std::mutex> jobLock(job->mutex);
                job->pendingWork = 1;
            }
            jobs_.push_back(job);
            work_.push_back({WorkKind::Scan, job, {}});
            pruneJobsLocked();
        }
        workReady_.notify_one();
        return job;
    }

    bool cancel(const std::shared_ptr<FileBatchJob>& job) {
        if (!job) {
            return false;
        }
        std::size_t removed = 0;
        {
            std::lock_guard<std::mutex> queueLock(queueMutex_);
            std::lock_guard<std::mutex> jobLock(job->mutex);
            if (isTerminal(job->state) ||
                job->state == FileBatchState::Cancelling) {
                return false;
            }
            job->cancellationRequested.store(true, std::memory_order_release);
            job->state = FileBatchState::Cancelling;
            job->results.clear();
            for (auto iterator = work_.begin(); iterator != work_.end();) {
                if (iterator->job == job) {
                    iterator = work_.erase(iterator);
                    ++removed;
                } else {
                    ++iterator;
                }
            }
            job->pendingWork =
                removed >= job->pendingWork ? 0 : job->pendingWork - removed;
            if (job->pendingWork == 0) {
                job->state = FileBatchState::Cancelled;
            }
        }
        job->resultSpace.notify_all();
        workReady_.notify_all();
        return true;
    }

    FileBatchSnapshot poll(const std::shared_ptr<FileBatchJob>& job,
                           std::size_t maximum) {
        if (!job) {
            throw std::invalid_argument("file batch job is invalid");
        }
        FileBatchSnapshot snapshot;
        {
            std::lock_guard<std::mutex> jobLock(job->mutex);
            snapshot.items.reserve(std::min(maximum, job->results.size()));
            while (snapshot.items.size() < maximum) {
                const auto iterator = job->results.find(job->nextDeliveryIndex);
                if (iterator == job->results.end()) {
                    break;
                }
                snapshot.items.push_back(std::move(iterator->second));
                job->results.erase(iterator);
                ++job->nextDeliveryIndex;
            }
            job->delivered += snapshot.items.size();
            snapshot.state = job->state;
            snapshot.total = job->total;
            snapshot.completed = job->completed;
            snapshot.delivered = job->delivered;
            snapshot.error = job->error;
            snapshot.drained = isTerminal(job->state) &&
                               job->pendingWork == 0 && job->results.empty();
        }
        job->resultSpace.notify_all();
        return snapshot;
    }

    void release(const std::shared_ptr<FileBatchJob>& job) noexcept {
        if (!job) {
            return;
        }
        job->cancellationRequested.store(true, std::memory_order_release);
        job->resultSpace.notify_all();
    }

    void shutdown() {
        bool expected = false;
        if (!stopping_.compare_exchange_strong(expected, true,
                                               std::memory_order_acq_rel)) {
            return;
        }

        std::vector<std::shared_ptr<FileBatchJob>> jobs;
        {
            std::lock_guard<std::mutex> queueLock(queueMutex_);
            for (const std::weak_ptr<FileBatchJob>& weakJob : jobs_) {
                if (std::shared_ptr<FileBatchJob> job = weakJob.lock()) {
                    jobs.push_back(std::move(job));
                }
            }
            work_.clear();
        }
        for (const std::shared_ptr<FileBatchJob>& job : jobs) {
            {
                std::lock_guard<std::mutex> jobLock(job->mutex);
                job->cancellationRequested.store(true,
                                                 std::memory_order_release);
                if (!isTerminal(job->state)) {
                    job->state = FileBatchState::Cancelling;
                }
            }
            job->resultSpace.notify_all();
        }
        workReady_.notify_all();
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
        for (const std::shared_ptr<FileBatchJob>& job : jobs) {
            std::lock_guard<std::mutex> jobLock(job->mutex);
            if (job->state == FileBatchState::Cancelling) {
                job->pendingWork = 0;
                job->state = FileBatchState::Cancelled;
            }
        }
    }

private:
    void ensureWorkersLocked() {
        if (!workers_.empty()) {
            return;
        }
        workers_.reserve(workerCount_);
        for (unsigned int index = 0; index < workerCount_; ++index) {
            workers_.emplace_back([this]() {
                workerLoop();
            });
        }
    }

    void pruneJobsLocked() {
        jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(),
                                   [](const std::weak_ptr<FileBatchJob>& job) {
                                       return job.expired();
                                   }),
                    jobs_.end());
    }

    void workerLoop() {
        while (true) {
            WorkItem workItem;
            {
                std::unique_lock<std::mutex> queueLock(queueMutex_);
                workReady_.wait(queueLock, [this]() {
                    return stopping_.load(std::memory_order_acquire) ||
                           !work_.empty();
                });
                if (stopping_.load(std::memory_order_acquire) && work_.empty()) {
                    return;
                }
                if (work_.empty()) {
                    continue;
                }
                workItem = std::move(work_.front());
                work_.pop_front();
            }
            if (workItem.kind == WorkKind::Scan) {
                handleScan(workItem.job);
            } else {
                handleRead(workItem.job, workItem.entry);
            }
        }
    }

    void handleScan(const std::shared_ptr<FileBatchJob>& job) {
        if (stopping_.load(std::memory_order_acquire) ||
            job->cancellationRequested.load(std::memory_order_acquire)) {
            finishCancelledWork(job);
            return;
        }
        ScanResult result = scanManifest(job);
        if (result.cancelled || stopping_.load(std::memory_order_acquire)) {
            finishCancelledWork(job);
            return;
        }
        if (result.error.has_value()) {
            finishFailure(job, std::move(*result.error));
            return;
        }

        {
            std::lock_guard<std::mutex> queueLock(queueMutex_);
            std::lock_guard<std::mutex> jobLock(job->mutex);
            if (stopping_.load(std::memory_order_acquire) ||
                job->cancellationRequested.load(std::memory_order_acquire)) {
                if (job->pendingWork > 0) {
                    --job->pendingWork;
                }
                if (job->state != FileBatchState::Failed) {
                    job->state = job->pendingWork == 0
                                     ? FileBatchState::Cancelled
                                     : FileBatchState::Cancelling;
                }
            } else {
                job->total = result.entries.size();
                if (job->pendingWork > 0) {
                    --job->pendingWork;
                }
                job->pendingWork += result.entries.size();
                if (result.entries.empty()) {
                    job->state = FileBatchState::Completed;
                } else {
                    job->state = FileBatchState::Running;
                    for (ManifestEntry& entry : result.entries) {
                        work_.push_back(
                            {WorkKind::Read, job, std::move(entry)});
                    }
                }
            }
        }
        job->resultSpace.notify_all();
        workReady_.notify_all();
    }

    void handleRead(const std::shared_ptr<FileBatchJob>& job,
                    const ManifestEntry& entry) {
        if (stopping_.load(std::memory_order_acquire) ||
            job->cancellationRequested.load(std::memory_order_acquire)) {
            finishCancelledWork(job);
            return;
        }
        ReadResult result = readFile(job, entry);
        if (result.cancelled || stopping_.load(std::memory_order_acquire)) {
            finishCancelledWork(job);
            return;
        }
        if (result.error.has_value()) {
            finishFailure(job, std::move(*result.error));
            removeQueuedWork(job);
            return;
        }

        std::unique_lock<std::mutex> jobLock(job->mutex);
        job->resultSpace.wait(jobLock, [this, &job, &result]() {
            return stopping_.load(std::memory_order_acquire) ||
                   job->cancellationRequested.load(std::memory_order_acquire) ||
                   job->results.size() < job->resultCapacity ||
                   result.item->index == job->nextDeliveryIndex;
        });
        if (stopping_.load(std::memory_order_acquire) ||
            job->cancellationRequested.load(std::memory_order_acquire)) {
            if (job->pendingWork > 0) {
                --job->pendingWork;
            }
            if (job->state != FileBatchState::Failed) {
                job->state = job->pendingWork == 0 ? FileBatchState::Cancelled
                                                   : FileBatchState::Cancelling;
            }
            return;
        }
        job->results.emplace(result.item->index, std::move(*result.item));
        ++job->completed;
        if (job->pendingWork > 0) {
            --job->pendingWork;
        }
        if (job->pendingWork == 0) {
            job->state = FileBatchState::Completed;
        }
    }

    void finishCancelledWork(const std::shared_ptr<FileBatchJob>& job) {
        std::lock_guard<std::mutex> jobLock(job->mutex);
        job->cancellationRequested.store(true, std::memory_order_release);
        if (job->pendingWork > 0) {
            --job->pendingWork;
        }
        if (job->state != FileBatchState::Failed) {
            job->state = job->pendingWork == 0 ? FileBatchState::Cancelled
                                               : FileBatchState::Cancelling;
        }
        job->resultSpace.notify_all();
    }

    void finishFailure(const std::shared_ptr<FileBatchJob>& job,
                       FileBatchError error) {
        std::lock_guard<std::mutex> jobLock(job->mutex);
        job->cancellationRequested.store(true, std::memory_order_release);
        if (!job->error.has_value()) {
            job->error = std::move(error);
        }
        job->state = FileBatchState::Failed;
        job->results.clear();
        if (job->pendingWork > 0) {
            --job->pendingWork;
        }
        job->resultSpace.notify_all();
    }

    void removeQueuedWork(const std::shared_ptr<FileBatchJob>& job) {
        std::size_t removed = 0;
        {
            std::lock_guard<std::mutex> queueLock(queueMutex_);
            for (auto iterator = work_.begin(); iterator != work_.end();) {
                if (iterator->job == job) {
                    iterator = work_.erase(iterator);
                    ++removed;
                } else {
                    ++iterator;
                }
            }
            std::lock_guard<std::mutex> jobLock(job->mutex);
            job->pendingWork =
                removed >= job->pendingWork ? 0 : job->pendingWork - removed;
        }
        job->resultSpace.notify_all();
    }

    std::atomic_bool stopping_ = false;
    unsigned int workerCount_ = 1;
    std::mutex queueMutex_;
    std::condition_variable workReady_;
    std::deque<WorkItem> work_;
    std::vector<std::thread> workers_;
    std::vector<std::weak_ptr<FileBatchJob>> jobs_;
};

FileBatchRuntime::FileBatchRuntime()
    : implementation_(std::make_unique<Implementation>()) {}

FileBatchRuntime::~FileBatchRuntime() = default;

std::shared_ptr<FileBatchJob> FileBatchRuntime::start(
    std::vector<FileBatchSpec> specs) {
    return implementation_->start(std::move(specs));
}

FileBatchSnapshot FileBatchRuntime::poll(
    const std::shared_ptr<FileBatchJob>& job, std::size_t maximum) {
    return implementation_->poll(job, maximum);
}

bool FileBatchRuntime::cancel(const std::shared_ptr<FileBatchJob>& job) {
    return implementation_->cancel(job);
}

void FileBatchRuntime::release(
    const std::shared_ptr<FileBatchJob>& job) noexcept {
    implementation_->release(job);
}

void FileBatchRuntime::shutdown() noexcept {
    implementation_->shutdown();
}

}  // namespace ludork::standard
