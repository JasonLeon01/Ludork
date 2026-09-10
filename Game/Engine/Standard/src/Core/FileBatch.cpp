#include "FileBatchRuntimeImpl.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
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

FileBatchRuntime::Impl::Impl() {
    const unsigned int detected = std::thread::hardware_concurrency();
    workerCount_ = std::min(4U, std::max(1U, detected));
}

FileBatchRuntime::Impl::~Impl() {
    clearJson();
}

std::shared_ptr<FileBatchJob> FileBatchRuntime::Impl::start(
    std::vector<FileBatchSpec> specs) {
    FileBatchJsonCallbacks callbacks = jsonRuntime_.callbacks();
    const bool needsJson =
        std::any_of(specs.begin(), specs.end(), [](const FileBatchSpec& spec) {
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

bool FileBatchRuntime::Impl::cancel(const std::shared_ptr<FileBatchJob>& job) {
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
    jsonRuntime_.deferDisposals(std::move(disposals));
    job->resultSpace.notify_all();
    workReady_.notify_all();
    return cancelled || clearedConversion || discardedResults;
}

FileBatchSnapshot FileBatchRuntime::Impl::poll(
    const std::shared_ptr<FileBatchJob>& job, std::size_t maximum) {
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
        snapshot.drained = isTerminal(job->state) && job->pendingWork == 0 &&
                           job->results.empty();
    }
    job->resultSpace.notify_all();
    return snapshot;
}

void FileBatchRuntime::Impl::release(
    const std::shared_ptr<FileBatchJob>& job) noexcept {
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

void FileBatchRuntime::Impl::configureJson(FileBatchJsonParser parser,
                                           FileBatchJsonBegin begin,
                                           FileBatchJsonStep step,
                                           FileBatchJsonClear clear) {
    jsonRuntime_.configure(std::move(parser), std::move(begin), std::move(step),
                           std::move(clear));
}

void FileBatchRuntime::Impl::clearJson() noexcept {
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

std::shared_ptr<FileBatchJsonConversionState>
FileBatchRuntime::Impl::beginJsonConversion(
    lua_State* state, const std::shared_ptr<FileBatchJob>& job,
    const FileBatchParsedJson& parsedJson) {
    return jsonRuntime_.beginConversion(state, job, parsedJson);
}

FileBatchJsonStepResult FileBatchRuntime::Impl::stepJsonConversion(
    lua_State* state,
    const std::shared_ptr<FileBatchJsonConversionState>& conversion,
    std::size_t maximumNodes, double maximumMilliseconds) {
    return jsonRuntime_.stepConversion(state, conversion, maximumNodes,
                                       maximumMilliseconds);
}

bool FileBatchRuntime::Impl::clearJsonConversion(
    const std::shared_ptr<FileBatchJsonConversionState>& conversion) noexcept {
    return jsonRuntime_.clearConversion(conversion);
}

void FileBatchRuntime::Impl::ensureWorkersLocked() {
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
            job->pendingWork += result.entries.size();
            if (result.entries.empty()) {
                job->state = FileBatchState::Completed;
            } else {
                job->state = FileBatchState::Running;
                for (ManifestEntry& entry : result.entries) {
                    work_.push_back({WorkKind::Read, job, std::move(entry)});
                }
            }
        }
    }
    job->resultSpace.notify_all();
    workReady_.notify_all();
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
    job->resultSpace.notify_all();
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
    if (job->pendingWork > 0) {
        --job->pendingWork;
    }
    job->resultSpace.notify_all();
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
    job->resultSpace.notify_all();
}

FileBatchRuntime::FileBatchRuntime() : impl_(std::make_unique<Impl>()) {}

FileBatchRuntime::~FileBatchRuntime() = default;

std::shared_ptr<FileBatchJob> FileBatchRuntime::start(
    std::vector<FileBatchSpec> specs) {
    return impl_->start(std::move(specs));
}

FileBatchSnapshot FileBatchRuntime::poll(
    const std::shared_ptr<FileBatchJob>& job, std::size_t maximum) {
    return impl_->poll(job, maximum);
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

void FileBatchRuntime::configureJson(FileBatchJsonParser parser,
                                     FileBatchJsonBegin begin,
                                     FileBatchJsonStep step,
                                     FileBatchJsonClear clear) {
    impl_->configureJson(std::move(parser), std::move(begin), std::move(step),
                         std::move(clear));
}

void FileBatchRuntime::clearJson() noexcept {
    impl_->clearJson();
}

std::shared_ptr<FileBatchJsonConversionState>
FileBatchRuntime::beginJsonConversion(lua_State* state,
                                      const std::shared_ptr<FileBatchJob>& job,
                                      const FileBatchParsedJson& parsedJson) {
    return impl_->beginJsonConversion(state, job, parsedJson);
}

FileBatchJsonStepResult FileBatchRuntime::stepJsonConversion(
    lua_State* state,
    const std::shared_ptr<FileBatchJsonConversionState>& conversion,
    std::size_t maximumNodes, double maximumMilliseconds) {
    return impl_->stepJsonConversion(state, conversion, maximumNodes,
                                     maximumMilliseconds);
}

bool FileBatchRuntime::clearJsonConversion(
    const std::shared_ptr<FileBatchJsonConversionState>& conversion) noexcept {
    return impl_->clearJsonConversion(conversion);
}

}  // namespace ludork::standard
