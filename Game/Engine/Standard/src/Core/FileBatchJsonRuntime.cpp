#include "FileBatchJsonRuntime.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

extern "C" {
#include <lua.h>
}

namespace ludork::standard {

FileBatchJsonRuntime::~FileBatchJsonRuntime() {
    reset();
}

FileBatchJsonCallbacks FileBatchJsonRuntime::callbacks() const {
    std::lock_guard<std::mutex> bridgeLock(bridgeMutex_);
    return {parser_, begin_, step_, clear_};
}

void FileBatchJsonRuntime::configure(FileBatchJsonParser parser,
                                     FileBatchJsonBegin begin,
                                     FileBatchJsonStep step,
                                     FileBatchJsonClear clear) {
    if (!parser || !begin || !step || !clear) {
        throw std::invalid_argument(
            "file batch JSON parser and conversion callbacks must be callable");
    }
    startDisposalWorker();
    std::lock_guard<std::mutex> bridgeLock(bridgeMutex_);
    parser_ = std::move(parser);
    begin_ = std::move(begin);
    step_ = std::move(step);
    clear_ = std::move(clear);
}

void FileBatchJsonRuntime::reset() noexcept {
    clearAllConversions();
    stopDisposalWorker();
    std::lock_guard<std::mutex> bridgeLock(bridgeMutex_);
    parser_ = {};
    begin_ = {};
    step_ = {};
    clear_ = {};
    conversions_.clear();
}

std::shared_ptr<FileBatchJsonConversionState>
FileBatchJsonRuntime::beginConversion(lua_State* state,
                                      const std::shared_ptr<FileBatchJob>& job,
                                      const FileBatchParsedJson& parsedJson) {
    if (state == nullptr || !job || !parsedJson) {
        throw std::invalid_argument(
            "file batch JSON conversion input is invalid");
    }
    FileBatchJsonBegin begin;
    FileBatchJsonStep step;
    FileBatchJsonClear clear;
    {
        std::lock_guard<std::mutex> jobLock(job->mutex);
        begin = job->jsonBegin;
        step = job->jsonStep;
        clear = job->jsonClear;
    }
    if (!begin || !step || !clear) {
        throw std::runtime_error(
            "file batch JSON conversion is not configured");
    }
    FileBatchJsonConversion conversion = begin(state, parsedJson);
    if (!conversion) {
        throw std::runtime_error(
            "file batch JSON conversion returned no state");
    }
    std::shared_ptr<FileBatchJsonConversionState> result =
        std::make_shared<FileBatchJsonConversionState>();
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

FileBatchJsonStepResult FileBatchJsonRuntime::stepConversion(
    lua_State* state,
    const std::shared_ptr<FileBatchJsonConversionState>& conversion,
    std::size_t maximumNodes, double maximumMilliseconds) {
    if (state == nullptr || !conversion) {
        throw std::invalid_argument("file batch JSON conversion is invalid");
    }
    if (maximumNodes == 0 || !(maximumMilliseconds > 0.0)) {
        throw std::invalid_argument(
            "file batch JSON conversion budgets must be positive");
    }

    const int stackBase = lua_gettop(state);
    FileBatchJsonConversion nativeConversion;
    FileBatchJsonClear clear;
    FileBatchJsonStepResult result;
    try {
        std::unique_lock<std::mutex> conversionLock(conversion->mutex);
        if (conversion->cleared) {
            throw std::runtime_error(
                conversion->completed
                    ? "file batch JSON conversion result was already delivered"
                    : "file batch JSON conversion was cleared");
        }
        result = conversion->step(state, conversion->conversion, maximumNodes,
                                  maximumMilliseconds);
        const int expectedTop = stackBase + (result.completed ? 1 : 0);
        if (lua_gettop(state) != expectedTop) {
            throw std::runtime_error(
                "file batch JSON conversion returned an invalid Lua stack "
                "result");
        }
        if (result.processedNodes > maximumNodes) {
            throw std::runtime_error(
                "file batch JSON conversion exceeded its node budget");
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

bool FileBatchJsonRuntime::clearConversion(
    const std::shared_ptr<FileBatchJsonConversionState>& conversion) noexcept {
    if (!conversion) {
        return false;
    }
    FileBatchJsonConversion nativeConversion;
    FileBatchJsonClear clear;
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

bool FileBatchJsonRuntime::clearConversionsForJob(
    const std::shared_ptr<FileBatchJob>& job) noexcept {
    bool cleared = false;
    for (const std::shared_ptr<FileBatchJsonConversionState>& conversion :
         liveConversions()) {
        if (conversion->job.lock() == job) {
            cleared = clearConversion(conversion) || cleared;
        }
    }
    return cleared;
}

void FileBatchJsonRuntime::clearAllConversions() noexcept {
    for (const std::shared_ptr<FileBatchJsonConversionState>& conversion :
         liveConversions()) {
        clearConversion(conversion);
    }
}

void FileBatchJsonRuntime::deferDisposals(
    std::vector<FileBatchJsonDisposal> disposals) noexcept {
    for (FileBatchJsonDisposal& disposal : disposals) {
        deferDisposal(std::move(disposal));
    }
}

void FileBatchJsonRuntime::startDisposalWorker() {
    std::lock_guard<std::mutex> disposalLock(disposalMutex_);
    if (disposalWorker_.joinable()) {
        return;
    }
    disposalStopping_ = false;
    disposalWorker_ = std::thread([this]() {
        disposalLoop();
    });
}

void FileBatchJsonRuntime::deferDisposal(
    FileBatchJsonDisposal disposal) noexcept {
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

void FileBatchJsonRuntime::stopDisposalWorker() noexcept {
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

void FileBatchJsonRuntime::disposalLoop() {
    while (true) {
        FileBatchJsonDisposal disposal;
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

void FileBatchJsonRuntime::pruneConversionsLocked() {
    conversions_.erase(
        std::remove_if(
            conversions_.begin(), conversions_.end(),
            [](const std::weak_ptr<FileBatchJsonConversionState>& conversion) {
                return conversion.expired();
            }),
        conversions_.end());
}

std::vector<std::shared_ptr<FileBatchJsonConversionState>>
FileBatchJsonRuntime::liveConversions() {
    std::vector<std::shared_ptr<FileBatchJsonConversionState>> result;
    std::lock_guard<std::mutex> bridgeLock(bridgeMutex_);
    for (const std::weak_ptr<FileBatchJsonConversionState>& weakConversion :
         conversions_) {
        if (std::shared_ptr<FileBatchJsonConversionState> conversion =
                weakConversion.lock()) {
            result.push_back(std::move(conversion));
        }
    }
    pruneConversionsLocked();
    return result;
}

void takeParsedJsonDisposals(std::map<std::size_t, FileBatchItem>& results,
                             std::vector<FileBatchJsonDisposal>& disposals) {
    for (auto& [index, item] : results) {
        static_cast<void>(index);
        if (item.parsedJson) {
            disposals.push_back(std::move(item.parsedJson));
        }
    }
}

}  // namespace ludork::standard
