#pragma once

#include "FileBatchInternal.hpp"
#include "FileBatchDataRuntime.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ludork::standard {

class FileBatchRuntime::Impl {
public:
    Impl();
    ~Impl();
    std::shared_ptr<FileBatchJob> start(std::vector<FileBatchSpec> specs);
    bool cancel(const std::shared_ptr<FileBatchJob>& job);
    FileBatchSnapshot poll(const std::shared_ptr<FileBatchJob>& job,
                           std::size_t maximum);
    void release(const std::shared_ptr<FileBatchJob>& job) noexcept;
    void shutdown();
    void configureData(FileBatchDataParser parser, FileBatchDataBegin begin,
                       FileBatchDataStep step, FileBatchDataClear clear);
    void clearData() noexcept;
    std::shared_ptr<FileBatchDataConversionState> beginDataConversion(
        lua_State* state, const std::shared_ptr<FileBatchJob>& job,
        const FileBatchParsedData& parsedData);
    FileBatchDataStepResult stepDataConversion(
        lua_State* state,
        const std::shared_ptr<FileBatchDataConversionState>& conversion,
        std::size_t maximumNodes, double maximumMilliseconds);
    bool clearDataConversion(
        const std::shared_ptr<FileBatchDataConversionState>&
            conversion) noexcept;

private:
    struct WorkItem {
        WorkKind kind = WorkKind::Scan;
        std::shared_ptr<FileBatchJob> job;
        ManifestEntry entry;
    };

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
    FileBatchDataRuntime dataRuntime_;
};

}  // namespace ludork::standard
