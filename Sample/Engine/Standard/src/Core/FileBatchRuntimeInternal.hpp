#pragma once

#include "FileBatchInternal.hpp"
#include "FileBatchJsonRuntime.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ludork::standard {

class FileBatchRuntime::Implementation {
public:
    Implementation();
    ~Implementation();
    std::shared_ptr<FileBatchJob> start(std::vector<FileBatchSpec> specs);
    bool cancel(const std::shared_ptr<FileBatchJob>& job);
    FileBatchSnapshot poll(const std::shared_ptr<FileBatchJob>& job,
                           std::size_t maximum);
    void release(const std::shared_ptr<FileBatchJob>& job) noexcept;
    void shutdown();
    void configureJson(FileBatchJsonParser parser, FileBatchJsonBegin begin,
                       FileBatchJsonStep step, FileBatchJsonClear clear);
    void clearJson() noexcept;
    std::shared_ptr<FileBatchJsonConversionState> beginJsonConversion(
        lua_State* state, const std::shared_ptr<FileBatchJob>& job,
        const FileBatchParsedJson& parsedJson);
    FileBatchJsonStepResult stepJsonConversion(
        lua_State* state,
        const std::shared_ptr<FileBatchJsonConversionState>& conversion,
        std::size_t maximumNodes, double maximumMilliseconds);
    bool clearJsonConversion(
        const std::shared_ptr<FileBatchJsonConversionState>&
            conversion) noexcept;

private:
    void ensureWorkersLocked();
    void pruneJobsLocked();
    void workerLoop();
    void handleScan(const std::shared_ptr<FileBatchJob>& job);
    void handleRead(const std::shared_ptr<FileBatchJob>& job,
                    const ManifestEntry& entry);
    void finishCancelledWork(const std::shared_ptr<FileBatchJob>& job);
    void finishFailure(const std::shared_ptr<FileBatchJob>& job,
                       FileBatchError error);
    void removeQueuedWork(const std::shared_ptr<FileBatchJob>& job);

    std::atomic_bool stopping_ = false;
    unsigned int workerCount_ = 1;
    std::mutex queueMutex_;
    std::condition_variable workReady_;
    std::deque<WorkItem> work_;
    std::vector<std::thread> workers_;
    std::vector<std::weak_ptr<FileBatchJob>> jobs_;
    FileBatchJsonRuntime jsonRuntime_;
};

}  // namespace ludork::standard
