#include "FileBatchRuntimeImpl.hpp"
#include <FileWorkers.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
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

FileBatchRuntime::Impl::Impl() : workerCount_(fileWorkerCount()) {}

FileBatchRuntime::Impl::~Impl() {
    clearData();
}

std::shared_ptr<FileBatchJob> FileBatchRuntime::Impl::start(
    std::vector<FileBatchSpec> specs) {
    FileBatchDataCallbacks callbacks = dataRuntime_.callbacks();
    const bool needsData =
        std::any_of(specs.begin(), specs.end(), [](const FileBatchSpec& spec) {
            return spec.parseData;
        });
    if (needsData && (!callbacks.parser || !callbacks.begin ||
                      !callbacks.step || !callbacks.clear)) {
        throw std::runtime_error(
            "file batch data conversion is not configured");
    }
    std::shared_ptr<FileBatchJob> job = std::make_shared<FileBatchJob>(
        std::move(specs), workerCount_ * 2, std::move(callbacks.parser),
        std::move(callbacks.begin), std::move(callbacks.step),
        std::move(callbacks.clear));
    {
        std::lock_guard<std::mutex> queueLock(queueMutex_);
        if (stopping_.load(std::memory_order_acquire)) {
            throw std::runtime_error("async worker is shutting down");
        }
        ensureWorkersLocked(work_.size() + 1);
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

bool FileBatchRuntime::Impl::cancel(const std::shared_ptr<FileBatchJob>& job) {
    if (!job) {
        return false;
    }
    const bool clearedConversion = dataRuntime_.clearConversionsForJob(job);
    std::vector<FileBatchDataDisposal> disposals;
    std::size_t removed = 0;
    bool cancelled = false;
    bool discardedResults = false;
    {
        std::lock_guard<std::mutex> queueLock(queueMutex_);
        std::lock_guard<std::mutex> jobLock(job->mutex);
        discardedResults = !job->results.empty();
        takeParsedDataDisposals(job->results, disposals);
        job->results.clear();
        job->unread.clear();
        if (isTerminal(job->state) ||
            job->state == FileBatchState::Cancelling) {
        } else {
            cancelled = true;
            job->cancellationRequested.store(true, std::memory_order_release);
            job->state = FileBatchState::Cancelling;
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
    }
    dataRuntime_.deferDisposals(std::move(disposals));
    job->resultReady.notify_all();
    workReady_.notify_all();
    return cancelled || clearedConversion || discardedResults;
}

FileBatchSnapshot FileBatchRuntime::Impl::poll(
    const std::shared_ptr<FileBatchJob>& job, std::size_t maximum,
    bool waitForItem) {
    BackgroundTask::poll();
    if (!job) {
        throw std::invalid_argument("file batch job is invalid");
    }
    FileBatchSnapshot snapshot;
    bool refill = false;
    {
        std::unique_lock<std::mutex> jobLock(job->mutex);
        if (waitForItem && maximum != 0) {
            BackgroundTask::wait(job->resultReady, jobLock, [&job] {
                return isTerminal(job->state) ||
                       job->results.contains(job->nextDeliveryIndex);
            });
        }
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
        snapshot.drained = isTerminal(job->state) && job->pendingWork == 0 &&
                           job->results.empty();
        refill = !snapshot.items.empty() && !job->unread.empty();
    }
    if (refill) {
        std::lock_guard queueLock(queueMutex_);
        std::lock_guard jobLock(job->mutex);
        if (job->state == FileBatchState::Running &&
            !stopping_.load(std::memory_order_acquire)) {
            queueReadsLocked(job);
            ensureWorkersLocked(work_.size());
            workReady_.notify_all();
        }
    }
    return snapshot;
}

void FileBatchRuntime::Impl::release(
    const std::shared_ptr<FileBatchJob>& job) noexcept {
    if (!job) {
        return;
    }
    job->cancellationRequested.store(true, std::memory_order_release);
    std::vector<FileBatchDataDisposal> disposals;
    {
        std::lock_guard<std::mutex> jobLock(job->mutex);
        takeParsedDataDisposals(job->results, disposals);
        job->results.clear();
        job->unread.clear();
    }
    dataRuntime_.deferDisposals(std::move(disposals));
    job->resultReady.notify_all();
}

void FileBatchRuntime::Impl::shutdown() {
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
            job->cancellationRequested.store(true, std::memory_order_release);
            job->unread.clear();
            if (!isTerminal(job->state)) {
                job->state = FileBatchState::Cancelling;
            }
        }
        job->resultReady.notify_all();
    }
    workReady_.notify_all();
    for (ludork::standard::BackgroundTask& worker : workers_) {
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
            job->resultReady.notify_all();
        }
    }
}

void FileBatchRuntime::Impl::configureData(FileBatchDataParser parser,
                                           FileBatchDataBegin begin,
                                           FileBatchDataStep step,
                                           FileBatchDataClear clear) {
    dataRuntime_.configure(std::move(parser), std::move(begin), std::move(step),
                           std::move(clear));
}

void FileBatchRuntime::Impl::clearData() noexcept {
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
    dataRuntime_.clearAllConversions();
    std::vector<FileBatchDataDisposal> disposals;
    for (const std::shared_ptr<FileBatchJob>& job : jobs) {
        std::lock_guard<std::mutex> jobLock(job->mutex);
        takeParsedDataDisposals(job->results, disposals);
        job->results.clear();
        job->dataParser = {};
        job->dataBegin = {};
        job->dataStep = {};
        job->dataClear = {};
    }
    dataRuntime_.deferDisposals(std::move(disposals));
    dataRuntime_.reset();
    stopping_.store(false, std::memory_order_release);
}

std::shared_ptr<FileBatchDataConversionState>
FileBatchRuntime::Impl::beginDataConversion(
    lua_State* state, const std::shared_ptr<FileBatchJob>& job,
    const FileBatchParsedData& parsedData) {
    return dataRuntime_.beginConversion(state, job, parsedData);
}

FileBatchDataStepResult FileBatchRuntime::Impl::stepDataConversion(
    lua_State* state,
    const std::shared_ptr<FileBatchDataConversionState>& conversion,
    std::size_t maximumNodes, double maximumMilliseconds) {
    return dataRuntime_.stepConversion(state, conversion, maximumNodes,
                                       maximumMilliseconds);
}

bool FileBatchRuntime::Impl::clearDataConversion(
    const std::shared_ptr<FileBatchDataConversionState>& conversion) noexcept {
    return dataRuntime_.clearConversion(conversion);
}

void FileBatchRuntime::Impl::ensureWorkersLocked(std::size_t queued) {
    const auto desired = std::min(workerCount_, busyWorkers_ + queued);
    while (workers_.size() < desired) {
        try {
            workers_.emplace_back([this] {
                workerLoop();
            });
        } catch (const std::system_error&) {
            if (workers_.empty()) {
                throw;
            }
            break;
        }
    }
}

void FileBatchRuntime::Impl::pruneJobsLocked() {
    jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(),
                               [](const std::weak_ptr<FileBatchJob>& job) {
                                   return job.expired();
                               }),
                jobs_.end());
}

void FileBatchRuntime::Impl::workerLoop() {
    while (true) {
        WorkItem workItem;
        {
            std::unique_lock<std::mutex> queueLock(queueMutex_);
            ludork::standard::BackgroundTask::wait(
                workReady_, queueLock, [this]() {
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
            ++busyWorkers_;
        }
        if (workItem.kind == WorkKind::Scan) {
            handleScan(workItem.job);
        } else {
            handleRead(workItem.job, workItem.entry);
        }
        {
            std::lock_guard lock(queueMutex_);
            --busyWorkers_;
        }
        BackgroundTask::yield();
    }
}

void FileBatchRuntime::Impl::handleScan(
    const std::shared_ptr<FileBatchJob>& job) {
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
                job->state = job->pendingWork == 0 ? FileBatchState::Cancelled
                                                   : FileBatchState::Cancelling;
            }
        } else {
            job->total = result.entries.size();
            if (job->pendingWork > 0) {
                --job->pendingWork;
            }
            if (result.entries.empty()) {
                job->state = FileBatchState::Completed;
            } else {
                job->state = FileBatchState::Running;
                job->unread = std::move(result.entries);
                queueReadsLocked(job);
                ensureWorkersLocked(work_.size() - 1);
            }
        }
    }
    job->resultReady.notify_all();
    workReady_.notify_all();
}

void FileBatchRuntime::Impl::queueReadsLocked(
    const std::shared_ptr<FileBatchJob>& job) {
    while (!job->unread.empty() &&
           job->pendingWork + job->results.size() < job->resultCapacity) {
        work_.push_back({WorkKind::Read, job, std::move(job->unread.front())});
        job->unread.pop_front();
        ++job->pendingWork;
    }
}

void FileBatchRuntime::Impl::handleRead(
    const std::shared_ptr<FileBatchJob>& job, const ManifestEntry& entry) {
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

    std::lock_guard jobLock(job->mutex);
    if (stopping_.load(std::memory_order_acquire) ||
        job->cancellationRequested.load(std::memory_order_acquire)) {
        if (job->pendingWork > 0) {
            --job->pendingWork;
        }
        if (job->state != FileBatchState::Failed) {
            job->state = job->pendingWork == 0 ? FileBatchState::Cancelled
                                               : FileBatchState::Cancelling;
        }
        job->resultReady.notify_all();
        return;
    }
    job->results.emplace(result.item->index, std::move(*result.item));
    ++job->completed;
    if (job->pendingWork > 0) {
        --job->pendingWork;
    }
    if (job->pendingWork == 0 && job->unread.empty()) {
        job->state = FileBatchState::Completed;
    }
    job->resultReady.notify_all();
}

void FileBatchRuntime::Impl::finishCancelledWork(
    const std::shared_ptr<FileBatchJob>& job) {
    std::lock_guard<std::mutex> jobLock(job->mutex);
    job->cancellationRequested.store(true, std::memory_order_release);
    if (job->pendingWork > 0) {
        --job->pendingWork;
    }
    if (job->state != FileBatchState::Failed) {
        job->state = job->pendingWork == 0 ? FileBatchState::Cancelled
                                           : FileBatchState::Cancelling;
    }
    job->resultReady.notify_all();
}

void FileBatchRuntime::Impl::finishFailure(
    const std::shared_ptr<FileBatchJob>& job, FileBatchError error) {
    std::lock_guard<std::mutex> jobLock(job->mutex);
    job->cancellationRequested.store(true, std::memory_order_release);
    if (!job->error.has_value()) {
        job->error = std::move(error);
    }
    job->state = FileBatchState::Failed;
    job->results.clear();
    job->unread.clear();
    if (job->pendingWork > 0) {
        --job->pendingWork;
    }
    job->resultReady.notify_all();
}

void FileBatchRuntime::Impl::removeQueuedWork(
    const std::shared_ptr<FileBatchJob>& job) {
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
    job->resultReady.notify_all();
}

FileBatchRuntime::FileBatchRuntime() : impl_(std::make_unique<Impl>()) {}

FileBatchRuntime::~FileBatchRuntime() = default;

std::shared_ptr<FileBatchJob> FileBatchRuntime::start(
    std::vector<FileBatchSpec> specs) {
    return impl_->start(std::move(specs));
}

FileBatchSnapshot FileBatchRuntime::poll(
    const std::shared_ptr<FileBatchJob>& job, std::size_t maximum,
    bool waitForItem) {
    return impl_->poll(job, maximum, waitForItem);
}

bool FileBatchRuntime::cancel(const std::shared_ptr<FileBatchJob>& job) {
    return impl_->cancel(job);
}

void FileBatchRuntime::release(
    const std::shared_ptr<FileBatchJob>& job) noexcept {
    impl_->release(job);
}

void FileBatchRuntime::shutdown() noexcept {
    impl_->shutdown();
}

void FileBatchRuntime::configureData(FileBatchDataParser parser,
                                     FileBatchDataBegin begin,
                                     FileBatchDataStep step,
                                     FileBatchDataClear clear) {
    impl_->configureData(std::move(parser), std::move(begin), std::move(step),
                         std::move(clear));
}

void FileBatchRuntime::clearData() noexcept {
    impl_->clearData();
}

std::shared_ptr<FileBatchDataConversionState>
FileBatchRuntime::beginDataConversion(lua_State* state,
                                      const std::shared_ptr<FileBatchJob>& job,
                                      const FileBatchParsedData& parsedData) {
    return impl_->beginDataConversion(state, job, parsedData);
}

FileBatchDataStepResult FileBatchRuntime::stepDataConversion(
    lua_State* state,
    const std::shared_ptr<FileBatchDataConversionState>& conversion,
    std::size_t maximumNodes, double maximumMilliseconds) {
    return impl_->stepDataConversion(state, conversion, maximumNodes,
                                     maximumMilliseconds);
}

bool FileBatchRuntime::clearDataConversion(
    const std::shared_ptr<FileBatchDataConversionState>& conversion) noexcept {
    return impl_->clearDataConversion(conversion);
}

}  // namespace ludork::standard
