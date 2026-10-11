#include "FileBatchDataRuntime.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

extern "C" {
#include <lua.h>
}

namespace ludork::standard {

FileBatchDataRuntime::~FileBatchDataRuntime() {
    reset();
}

FileBatchDataCallbacks FileBatchDataRuntime::callbacks() const {
    std::lock_guard<std::mutex> bridgeLock(bridgeMutex_);
    return {parser_, begin_, step_, clear_};
}

void FileBatchDataRuntime::configure(FileBatchDataParser parser,
                                     FileBatchDataBegin begin,
                                     FileBatchDataStep step,
                                     FileBatchDataClear clear) {
    if (!parser || !begin || !step || !clear) {
        throw std::invalid_argument(
            "file batch data parser and conversion callbacks must be callable");
    }
    startDisposalWorker();
    std::lock_guard<std::mutex> bridgeLock(bridgeMutex_);
    parser_ = std::move(parser);
    begin_ = std::move(begin);
    step_ = std::move(step);
    clear_ = std::move(clear);
}

void FileBatchDataRuntime::reset() noexcept {
    clearAllConversions();
    stopDisposalWorker();
    std::lock_guard<std::mutex> bridgeLock(bridgeMutex_);
    parser_ = {};
    begin_ = {};
    step_ = {};
    clear_ = {};
    conversions_.clear();
}

std::shared_ptr<FileBatchDataConversionState>
FileBatchDataRuntime::beginConversion(lua_State* state,
                                      const std::shared_ptr<FileBatchJob>& job,
                                      const FileBatchParsedData& parsedData) {
    if (state == nullptr || !job || !parsedData) {
        throw std::invalid_argument(
            "file batch data conversion input is invalid");
    }
    FileBatchDataBegin begin;
    FileBatchDataStep step;
    FileBatchDataClear clear;
    {
        std::lock_guard<std::mutex> jobLock(job->mutex);
        begin = job->dataBegin;
        step = job->dataStep;
        clear = job->dataClear;
    }
    if (!begin || !step || !clear) {
        throw std::runtime_error(
            "file batch data conversion is not configured");
    }
    FileBatchDataConversion conversion = begin(state, parsedData);
    if (!conversion) {
        throw std::runtime_error(
            "file batch data conversion returned no state");
    }
    std::shared_ptr<FileBatchDataConversionState> result =
        std::make_shared<FileBatchDataConversionState>();
    result->job = job;
    result->conversion = std::move(conversion);
    result->step = std::move(step);
    result->clear = std::move(clear);
    {
        std::lock_guard<std::mutex> bridgeLock(bridgeMutex_);
        conversions_.push_back(result);
        pruneConversionsLocked();
    }
    return result;
}

FileBatchDataStepResult FileBatchDataRuntime::stepConversion(
    lua_State* state,
    const std::shared_ptr<FileBatchDataConversionState>& conversion,
    std::size_t maximumNodes, double maximumMilliseconds) {
    if (state == nullptr || !conversion) {
        throw std::invalid_argument("file batch data conversion is invalid");
    }
    if (maximumNodes == 0 || !(maximumMilliseconds > 0.0)) {
        throw std::invalid_argument(
            "file batch data conversion budgets must be positive");
    }

    const int stackBase = lua_gettop(state);
    FileBatchDataConversion nativeConversion;
    FileBatchDataClear clear;
    FileBatchDataStepResult result;
    try {
        std::unique_lock<std::mutex> conversionLock(conversion->mutex);
        if (conversion->cleared) {
            throw std::runtime_error(
                conversion->completed
                    ? "file batch data conversion result was already delivered"
                    : "file batch data conversion was cleared");
        }
        result = conversion->step(state, conversion->conversion, maximumNodes,
                                  maximumMilliseconds);
        const int expectedTop = stackBase + (result.completed ? 1 : 0);
        if (lua_gettop(state) != expectedTop) {
            throw std::runtime_error(
                "file batch data conversion returned an invalid Lua stack "
                "result");
        }
        if (result.processedNodes > maximumNodes) {
            throw std::runtime_error(
                "file batch data conversion exceeded its node budget");
        }
        if (result.completed) {
            conversion->completed = true;
            conversion->cleared = true;
            nativeConversion = std::move(conversion->conversion);
            clear = std::move(conversion->clear);
            conversion->step = {};
        }
    } catch (...) {
        lua_settop(state, stackBase);
        clearConversion(conversion);
        throw;
    }
    if (clear && nativeConversion) {
        try {
            deferDisposal(clear(nativeConversion));
        } catch (...) {}
    }
    return result;
}

bool FileBatchDataRuntime::clearConversion(
    const std::shared_ptr<FileBatchDataConversionState>& conversion) noexcept {
    if (!conversion) {
        return false;
    }
    FileBatchDataConversion nativeConversion;
    FileBatchDataClear clear;
    {
        std::lock_guard<std::mutex> conversionLock(conversion->mutex);
        if (conversion->cleared) {
            return false;
        }
        conversion->cleared = true;
        nativeConversion = std::move(conversion->conversion);
        clear = std::move(conversion->clear);
        conversion->step = {};
    }
    if (clear && nativeConversion) {
        try {
            deferDisposal(clear(nativeConversion));
        } catch (...) {}
    }
    return true;
}

bool FileBatchDataRuntime::clearConversionsForJob(
    const std::shared_ptr<FileBatchJob>& job) noexcept {
    bool cleared = false;
    for (const std::shared_ptr<FileBatchDataConversionState>& conversion :
         liveConversions()) {
        if (conversion->job.lock() == job) {
            cleared = clearConversion(conversion) || cleared;
        }
    }
    return cleared;
}

void FileBatchDataRuntime::clearAllConversions() noexcept {
    for (const std::shared_ptr<FileBatchDataConversionState>& conversion :
         liveConversions()) {
        clearConversion(conversion);
    }
}

void FileBatchDataRuntime::deferDisposals(
    std::vector<FileBatchDataDisposal> disposals) noexcept {
    for (FileBatchDataDisposal& disposal : disposals) {
        deferDisposal(std::move(disposal));
    }
}

void FileBatchDataRuntime::startDisposalWorker() {
    std::lock_guard<std::mutex> disposalLock(disposalMutex_);
    if (disposalWorker_.joinable()) {
        return;
    }
    disposalStopping_ = false;
    disposalWorker_ = std::thread([this]() {
        disposalLoop();
    });
}

void FileBatchDataRuntime::deferDisposal(
    FileBatchDataDisposal disposal) noexcept {
    if (!disposal) {
        return;
    }
    try {
        {
            std::lock_guard<std::mutex> disposalLock(disposalMutex_);
            if (disposalStopping_) {
                return;
            }
            disposals_.push_back(std::move(disposal));
        }
        disposalReady_.notify_one();
    } catch (...) {}
}

void FileBatchDataRuntime::stopDisposalWorker() noexcept {
    {
        std::lock_guard<std::mutex> disposalLock(disposalMutex_);
        disposalStopping_ = true;
    }
    disposalReady_.notify_all();
    if (disposalWorker_.joinable()) {
        disposalWorker_.join();
    }
    std::lock_guard<std::mutex> disposalLock(disposalMutex_);
    disposals_.clear();
}

void FileBatchDataRuntime::disposalLoop() {
    while (true) {
        FileBatchDataDisposal disposal;
        {
            std::unique_lock<std::mutex> disposalLock(disposalMutex_);
            disposalReady_.wait(disposalLock, [this]() {
                return disposalStopping_ || !disposals_.empty();
            });
            if (disposals_.empty()) {
                if (disposalStopping_) {
                    return;
                }
                continue;
            }
            disposal = std::move(disposals_.front());
            disposals_.pop_front();
        }
        disposal.reset();
    }
}

void FileBatchDataRuntime::pruneConversionsLocked() {
    conversions_.erase(
        std::remove_if(
            conversions_.begin(), conversions_.end(),
            [](const std::weak_ptr<FileBatchDataConversionState>& conversion) {
                return conversion.expired();
            }),
        conversions_.end());
}

std::vector<std::shared_ptr<FileBatchDataConversionState>>
FileBatchDataRuntime::liveConversions() {
    std::vector<std::shared_ptr<FileBatchDataConversionState>> result;
    std::lock_guard<std::mutex> bridgeLock(bridgeMutex_);
    for (const std::weak_ptr<FileBatchDataConversionState>& weakConversion :
         conversions_) {
        if (std::shared_ptr<FileBatchDataConversionState> conversion =
                weakConversion.lock()) {
            result.push_back(std::move(conversion));
        }
    }
    pruneConversionsLocked();
    return result;
}

void takeParsedDataDisposals(std::map<std::size_t, FileBatchItem>& results,
                             std::vector<FileBatchDataDisposal>& disposals) {
    for (auto& [index, item] : results) {
        static_cast<void>(index);
        if (item.parsedData) {
            disposals.push_back(std::move(item.parsedData));
        }
    }
}

}  // namespace ludork::standard
