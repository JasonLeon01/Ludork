#pragma once

#include "FileBatchInternal.hpp"

#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ludork::standard {

struct FileBatchDataCallbacks {
    FileBatchDataParser parser;
    FileBatchDataBegin begin;
    FileBatchDataStep step;
    FileBatchDataClear clear;
};

class FileBatchDataRuntime {
public:
    FileBatchDataRuntime() = default;
    ~FileBatchDataRuntime();

    FileBatchDataRuntime(const FileBatchDataRuntime&) = delete;
    FileBatchDataRuntime& operator=(const FileBatchDataRuntime&) = delete;

    FileBatchDataCallbacks callbacks() const;
    void configure(FileBatchDataParser parser, FileBatchDataBegin begin,
                   FileBatchDataStep step, FileBatchDataClear clear);
    void reset() noexcept;

    std::shared_ptr<FileBatchDataConversionState> beginConversion(
        lua_State* state, const std::shared_ptr<FileBatchJob>& job,
        const FileBatchParsedData& parsedData);
    FileBatchDataStepResult stepConversion(
        lua_State* state,
        const std::shared_ptr<FileBatchDataConversionState>& conversion,
        std::size_t maximumNodes, double maximumMilliseconds);
    bool clearConversion(const std::shared_ptr<FileBatchDataConversionState>&
                             conversion) noexcept;
    bool clearConversionsForJob(
        const std::shared_ptr<FileBatchJob>& job) noexcept;
    void clearAllConversions() noexcept;

    void deferDisposals(std::vector<FileBatchDataDisposal> disposals) noexcept;

private:
    void startDisposalWorker();
    void deferDisposal(FileBatchDataDisposal disposal) noexcept;
    void stopDisposalWorker() noexcept;
    void disposalLoop();
    void pruneConversionsLocked();
    std::vector<std::shared_ptr<FileBatchDataConversionState>>
    liveConversions();

    mutable std::mutex bridgeMutex_;
    FileBatchDataParser parser_;
    FileBatchDataBegin begin_;
    FileBatchDataStep step_;
    FileBatchDataClear clear_;
    std::vector<std::weak_ptr<FileBatchDataConversionState>> conversions_;
    std::mutex disposalMutex_;
    std::condition_variable disposalReady_;
    std::deque<FileBatchDataDisposal> disposals_;
    std::thread disposalWorker_;
    bool disposalStopping_ = true;
};

void takeParsedDataDisposals(std::map<std::size_t, FileBatchItem>& results,
                             std::vector<FileBatchDataDisposal>& disposals);

}  // namespace ludork::standard
