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

struct FileBatchJsonCallbacks {
    FileBatchJsonParser parser;
    FileBatchJsonBegin begin;
    FileBatchJsonStep step;
    FileBatchJsonClear clear;
};

class FileBatchJsonRuntime {
public:
    FileBatchJsonRuntime() = default;
    ~FileBatchJsonRuntime();

    FileBatchJsonRuntime(const FileBatchJsonRuntime&) = delete;
    FileBatchJsonRuntime& operator=(const FileBatchJsonRuntime&) = delete;

    FileBatchJsonCallbacks callbacks() const;
    void configure(FileBatchJsonParser parser, FileBatchJsonBegin begin,
                   FileBatchJsonStep step, FileBatchJsonClear clear);
    void reset() noexcept;

    std::shared_ptr<FileBatchJsonConversionState> beginConversion(
        lua_State* state, const std::shared_ptr<FileBatchJob>& job,
        const FileBatchParsedJson& parsedJson);
    FileBatchJsonStepResult stepConversion(
        lua_State* state,
        const std::shared_ptr<FileBatchJsonConversionState>& conversion,
        std::size_t maximumNodes, double maximumMilliseconds);
    bool clearConversion(const std::shared_ptr<FileBatchJsonConversionState>&
                             conversion) noexcept;
    bool clearConversionsForJob(
        const std::shared_ptr<FileBatchJob>& job) noexcept;
    void clearAllConversions() noexcept;

    void deferDisposals(std::vector<FileBatchJsonDisposal> disposals) noexcept;

private:
    void startDisposalWorker();
    void deferDisposal(FileBatchJsonDisposal disposal) noexcept;
    void stopDisposalWorker() noexcept;
    void disposalLoop();
    void pruneConversionsLocked();
    std::vector<std::shared_ptr<FileBatchJsonConversionState>>
    liveConversions();

    mutable std::mutex bridgeMutex_;
    FileBatchJsonParser parser_;
    FileBatchJsonBegin begin_;
    FileBatchJsonStep step_;
    FileBatchJsonClear clear_;
    std::vector<std::weak_ptr<FileBatchJsonConversionState>> conversions_;
    std::mutex disposalMutex_;
    std::condition_variable disposalReady_;
    std::deque<FileBatchJsonDisposal> disposals_;
    std::thread disposalWorker_;
    bool disposalStopping_ = true;
};

void takeParsedJsonDisposals(std::map<std::size_t, FileBatchItem>& results,
                             std::vector<FileBatchJsonDisposal>& disposals);

}  // namespace ludork::standard
