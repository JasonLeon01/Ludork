#include "FileBatchInternal.hpp"
#include "FileBatchJsonRuntime.hpp"

#include <DataFile.hpp>
#include <ReadOnlyFileProvider.hpp>
#include <Utf8Path.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace ludork::standard {

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

}  // namespace

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

}  // namespace

class FileBatchRuntime::Implementation {
public:
    Implementation() {
        const unsigned int detected = std::thread::hardware_concurrency();
        workerCount_ = std::min(4U, std::max(1U, detected));
    }

    ~Implementation() {
        clearJson();
    }

    std::shared_ptr<FileBatchJob> start(std::vector<FileBatchSpec> specs) {
        FileBatchJsonCallbacks callbacks = jsonRuntime_.callbacks();
        const bool needsJson = std::any_of(specs.begin(), specs.end(),
                                           [](const FileBatchSpec& spec) {
                                               return spec.parseJson;
                                           });
        if (needsJson && (!callbacks.parser || !callbacks.begin ||
                          !callbacks.step || !callbacks.clear)) {
            throw std::runtime_error(
                "file batch JSON conversion is not configured");
        }
        std::shared_ptr<FileBatchJob> job = std::make_shared<FileBatchJob>(
            std::move(specs), static_cast<std::size_t>(workerCount_) * 2,
            std::move(callbacks.parser), std::move(callbacks.begin),
            std::move(callbacks.step), std::move(callbacks.clear));
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
        const bool clearedConversion = jsonRuntime_.clearConversionsForJob(job);
        std::vector<FileBatchJsonDisposal> disposals;
        std::size_t removed = 0;
        bool cancelled = false;
        bool discardedResults = false;
        {
            std::lock_guard<std::mutex> queueLock(queueMutex_);
            std::lock_guard<std::mutex> jobLock(job->mutex);
            discardedResults = !job->results.empty();
            takeParsedJsonDisposals(job->results, disposals);
            job->results.clear();
            if (isTerminal(job->state) ||
                job->state == FileBatchState::Cancelling) {
            } else {
                cancelled = true;
                job->cancellationRequested.store(true,
                                                 std::memory_order_release);
                job->state = FileBatchState::Cancelling;
                for (auto iterator = work_.begin(); iterator != work_.end();) {
                    if (iterator->job == job) {
                        iterator = work_.erase(iterator);
                        ++removed;
                    } else {
                        ++iterator;
                    }
                }
                job->pendingWork = removed >= job->pendingWork
                                       ? 0
                                       : job->pendingWork - removed;
                if (job->pendingWork == 0) {
                    job->state = FileBatchState::Cancelled;
                }
            }
        }
        jsonRuntime_.deferDisposals(std::move(disposals));
        job->resultSpace.notify_all();
        workReady_.notify_all();
        return cancelled || clearedConversion || discardedResults;
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
        std::vector<FileBatchJsonDisposal> disposals;
        {
            std::lock_guard<std::mutex> jobLock(job->mutex);
            takeParsedJsonDisposals(job->results, disposals);
            job->results.clear();
        }
        jsonRuntime_.deferDisposals(std::move(disposals));
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

    void configureJson(FileBatchJsonParser parser, FileBatchJsonBegin begin,
                       FileBatchJsonStep step, FileBatchJsonClear clear) {
        jsonRuntime_.configure(std::move(parser), std::move(begin),
                               std::move(step), std::move(clear));
    }

    void clearJson() noexcept {
        shutdown();
        std::vector<std::shared_ptr<FileBatchJob>> jobs;
        {
            std::lock_guard<std::mutex> queueLock(queueMutex_);
            for (const std::weak_ptr<FileBatchJob>& weakJob : jobs_) {
                if (std::shared_ptr<FileBatchJob> job = weakJob.lock()) {
                    jobs.push_back(std::move(job));
                }
            }
            jobs_.clear();
            work_.clear();
        }
        jsonRuntime_.clearAllConversions();
        std::vector<FileBatchJsonDisposal> disposals;
        for (const std::shared_ptr<FileBatchJob>& job : jobs) {
            std::lock_guard<std::mutex> jobLock(job->mutex);
            takeParsedJsonDisposals(job->results, disposals);
            job->results.clear();
            job->jsonParser = {};
            job->jsonBegin = {};
            job->jsonStep = {};
            job->jsonClear = {};
        }
        jsonRuntime_.deferDisposals(std::move(disposals));
        jsonRuntime_.reset();
        stopping_.store(false, std::memory_order_release);
    }

    std::shared_ptr<FileBatchJsonConversionState> beginJsonConversion(
        lua_State* state, const std::shared_ptr<FileBatchJob>& job,
        const FileBatchParsedJson& parsedJson) {
        return jsonRuntime_.beginConversion(state, job, parsedJson);
    }

    FileBatchJsonStepResult stepJsonConversion(
        lua_State* state,
        const std::shared_ptr<FileBatchJsonConversionState>& conversion,
        std::size_t maximumNodes, double maximumMilliseconds) {
        return jsonRuntime_.stepConversion(state, conversion, maximumNodes,
                                           maximumMilliseconds);
    }

    bool clearJsonConversion(
        const std::shared_ptr<FileBatchJsonConversionState>&
            conversion) noexcept {
        return jsonRuntime_.clearConversion(conversion);
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
                if (stopping_.load(std::memory_order_acquire) &&
                    work_.empty()) {
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
    FileBatchJsonRuntime jsonRuntime_;
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

void FileBatchRuntime::configureJson(FileBatchJsonParser parser,
                                     FileBatchJsonBegin begin,
                                     FileBatchJsonStep step,
                                     FileBatchJsonClear clear) {
    implementation_->configureJson(std::move(parser), std::move(begin),
                                   std::move(step), std::move(clear));
}

void FileBatchRuntime::clearJson() noexcept {
    implementation_->clearJson();
}

std::shared_ptr<FileBatchJsonConversionState>
FileBatchRuntime::beginJsonConversion(lua_State* state,
                                      const std::shared_ptr<FileBatchJob>& job,
                                      const FileBatchParsedJson& parsedJson) {
    return implementation_->beginJsonConversion(state, job, parsedJson);
}

FileBatchJsonStepResult FileBatchRuntime::stepJsonConversion(
    lua_State* state,
    const std::shared_ptr<FileBatchJsonConversionState>& conversion,
    std::size_t maximumNodes, double maximumMilliseconds) {
    return implementation_->stepJsonConversion(state, conversion, maximumNodes,
                                               maximumMilliseconds);
}

bool FileBatchRuntime::clearJsonConversion(
    const std::shared_ptr<FileBatchJsonConversionState>& conversion) noexcept {
    return implementation_->clearJsonConversion(conversion);
}

}  // namespace ludork::standard
