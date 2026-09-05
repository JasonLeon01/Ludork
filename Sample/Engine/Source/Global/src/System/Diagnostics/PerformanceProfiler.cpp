#include "PerformanceProfiler.hpp"

#include <SystemServices.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

std::atomic_bool PerformanceProfiler::enabled_ = false;
std::atomic_uint64_t PerformanceProfiler::generation_ = 0;
std::mutex PerformanceProfiler::mutex_;
std::chrono::steady_clock::time_point PerformanceProfiler::epoch_;
std::optional<std::chrono::steady_clock::time_point>
    PerformanceProfiler::previousMainFrameStart_;
std::optional<std::chrono::steady_clock::time_point>
    PerformanceProfiler::batchStart_;
std::vector<PerformanceProfiler::MainFrameRecord>
    PerformanceProfiler::mainFrames_;
std::vector<PerformanceProfiler::LogicTickRecord>
    PerformanceProfiler::logicTicks_;
std::uint64_t PerformanceProfiler::droppedLogicTicks_ = 0;
thread_local double PerformanceProfiler::presentWaitMilliseconds_ = 0.0;
thread_local std::optional<PerformanceProfiler::PendingWorldStreamingRecord>
    PerformanceProfiler::pendingWorldStreaming_;

bool PerformanceProfiler::isEnabled() noexcept {
    return enabled_.load(std::memory_order_acquire);
}

void PerformanceProfiler::setEnabled(bool enabled) {
    const std::lock_guard<std::mutex> lock(mutex_);
    enabled_.store(false, std::memory_order_release);
    resetLocked(std::chrono::steady_clock::now());
    enabled_.store(enabled, std::memory_order_release);
}

void PerformanceProfiler::shutdown() noexcept {
    const std::lock_guard<std::mutex> lock(mutex_);
    enabled_.store(false, std::memory_order_release);
    resetLocked({});
}

void PerformanceProfiler::beginMainFrame() noexcept {
    presentWaitMilliseconds_ = 0.0;
}

void PerformanceProfiler::addPresentWait(double millisecondsValue) noexcept {
    if (!isEnabled() || !std::isfinite(millisecondsValue) ||
        millisecondsValue <= 0.0) {
        return;
    }
    presentWaitMilliseconds_ += millisecondsValue;
}

void PerformanceProfiler::recordMainFrame(
    const MainFramePerformanceMeasurement& measurement) {
    if (!isEnabled()) {
        return;
    }
    std::optional<Batch> completedBatch;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_.load(std::memory_order_relaxed) ||
            measurement.start < epoch_) {
            return;
        }
        MainFrameRecord record;
        record.timeSeconds = std::max(0.0, seconds(measurement.start - epoch_));
        record.activeMilliseconds =
            std::max(0.0, milliseconds(measurement.end - measurement.start));
        record.intervalMilliseconds =
            previousMainFrameStart_.has_value()
                ? std::max(0.0, milliseconds(measurement.start -
                                             *previousMainFrameStart_))
                : record.activeMilliseconds;
        record.runtimeMilliseconds =
            std::max(0.0, measurement.runtimeMilliseconds);
        record.sceneOperationsMilliseconds =
            std::max(0.0, measurement.sceneOperationsMilliseconds);
        record.inputMilliseconds = std::max(0.0, measurement.inputMilliseconds);
        record.uiUpdateMilliseconds =
            std::max(0.0, measurement.uiUpdateMilliseconds);
        record.presentWaitMilliseconds =
            std::max(0.0, presentWaitMilliseconds_);
        record.renderCpuMilliseconds =
            std::max(0.0, measurement.renderMilliseconds -
                              record.presentWaitMilliseconds);
        record.lateUpdateMilliseconds =
            std::max(0.0, measurement.lateUpdateMilliseconds);
        record.audioMilliseconds = std::max(0.0, measurement.audioMilliseconds);
        presentWaitMilliseconds_ = 0.0;
        previousMainFrameStart_ = measurement.start;
        if (!batchStart_.has_value()) {
            batchStart_ = measurement.start;
        }
        mainFrames_.push_back(record);
        if (mainFrames_.size() == sampleFrameCount_) {
            const double elapsedSeconds =
                seconds(measurement.end - *batchStart_);
            Batch batch;
            batch.generation = generation_.load(std::memory_order_relaxed);
            batch.fps =
                elapsedSeconds > 0.0
                    ? static_cast<double>(sampleFrameCount_) / elapsedSeconds
                    : 0.0;
            batch.memoryMegabytes = ludork::standard::processMemoryMegabytes();
            batch.targetFps = measurement.targetFps;
            batch.mainFrames.swap(mainFrames_);
            batch.logicTicks.swap(logicTicks_);
            batch.droppedLogicTicks = std::exchange(droppedLogicTicks_, 0);
            batchStart_.reset();
            completedBatch = std::move(batch);
        }
    }
    if (completedBatch.has_value()) {
        emit(std::move(*completedBatch));
    }
}

void PerformanceProfiler::recordLogicTick(
    const LogicTickPerformanceMeasurement& measurement) {
    std::optional<PendingWorldStreamingRecord> pendingWorldStreaming =
        std::exchange(pendingWorldStreaming_, std::nullopt);
    if (!isEnabled()) {
        return;
    }
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_.load(std::memory_order_relaxed) ||
        measurement.start < epoch_) {
        return;
    }
    if (logicTicks_.size() >= maximumLogicTickCount_) {
        ++droppedLogicTicks_;
        return;
    }
    LogicTickRecord record;
    record.timeSeconds = std::max(0.0, seconds(measurement.start - epoch_));
    record.iterationMilliseconds =
        std::max(0.0, milliseconds(measurement.end - measurement.start));
    record.sceneTickMilliseconds =
        std::max(0.0, measurement.sceneTickMilliseconds);
    record.maintenanceMilliseconds =
        std::max(0.0, measurement.maintenanceMilliseconds);
    record.fixedTickMilliseconds =
        std::max(0.0, measurement.fixedTickMilliseconds);
    record.sleepMilliseconds = std::max(0.0, measurement.sleepMilliseconds);
    record.fixedSteps = std::max(0, measurement.fixedSteps);
    if (pendingWorldStreaming.has_value() &&
        pendingWorldStreaming->generation ==
            generation_.load(std::memory_order_relaxed)) {
        record.worldStreaming = pendingWorldStreaming->measurement;
    }
    logicTicks_.push_back(record);
}

void PerformanceProfiler::recordWorldStreaming(
    const WorldStreamingPerformanceMeasurement& measurement) {
    if (!isEnabled()) {
        return;
    }
    const auto nonnegative = [](int value) {
        return std::max(0, value);
    };
    PendingWorldStreamingRecord record;
    record.generation = generation_.load(std::memory_order_acquire);
    record.measurement.queueDepth = nonnegative(measurement.queueDepth);
    record.measurement.reading = nonnegative(measurement.reading);
    record.measurement.prepared = nonnegative(measurement.prepared);
    record.measurement.active = nonnegative(measurement.active);
    record.measurement.dormant = nonnegative(measurement.dormant);
    record.measurement.cacheBytes = static_cast<std::uint64_t>(
        std::max<std::int64_t>(0, measurement.cacheBytes));
    record.measurement.publishMilliseconds =
        std::isfinite(measurement.publishMilliseconds)
            ? std::max(0.0, measurement.publishMilliseconds)
            : 0.0;
    record.measurement.visibleTileChunks =
        nonnegative(measurement.visibleTileChunks);
    record.measurement.activeActors = nonnegative(measurement.activeActors);
    pendingWorldStreaming_ = std::move(record);
}

void PerformanceProfiler::resetLocked(
    std::chrono::steady_clock::time_point epoch) {
    epoch_ = epoch;
    previousMainFrameStart_.reset();
    batchStart_.reset();
    mainFrames_.clear();
    logicTicks_.clear();
    droppedLogicTicks_ = 0;
    presentWaitMilliseconds_ = 0.0;
    generation_.fetch_add(1, std::memory_order_release);
}

void PerformanceProfiler::emit(Batch batch) {
    const auto finite = [](double value) {
        return std::isfinite(value) ? std::max(0.0, value) : 0.0;
    };
    std::ostringstream output;
    output << std::setprecision(10)
           << "__LUDORK_PERF__:{\"v\":2,\"fps\":" << finite(batch.fps)
           << ",\"memory\":" << finite(batch.memoryMegabytes)
           << ",\"sampleFrames\":" << sampleFrameCount_ << ",\"targetFps\":";
    if (batch.targetFps > 0) {
        output << batch.targetFps;
    } else {
        output << "null";
    }
    output << ",\"mainFrames\":[";
    for (std::size_t index = 0; index < batch.mainFrames.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const MainFrameRecord& record = batch.mainFrames[index];
        output << "{\"time\":" << finite(record.timeSeconds)
               << ",\"interval\":" << finite(record.intervalMilliseconds)
               << ",\"active\":" << finite(record.activeMilliseconds)
               << ",\"runtime\":" << finite(record.runtimeMilliseconds)
               << ",\"sceneOps\":" << finite(record.sceneOperationsMilliseconds)
               << ",\"input\":" << finite(record.inputMilliseconds)
               << ",\"uiUpdate\":" << finite(record.uiUpdateMilliseconds)
               << ",\"renderCpu\":" << finite(record.renderCpuMilliseconds)
               << ",\"presentWait\":" << finite(record.presentWaitMilliseconds)
               << ",\"lateUpdate\":" << finite(record.lateUpdateMilliseconds)
               << ",\"audio\":" << finite(record.audioMilliseconds) << '}';
    }
    output << "],\"logicTicks\":[";
    for (std::size_t index = 0; index < batch.logicTicks.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const LogicTickRecord& record = batch.logicTicks[index];
        output << "{\"time\":" << finite(record.timeSeconds)
               << ",\"iteration\":" << finite(record.iterationMilliseconds)
               << ",\"sceneTick\":" << finite(record.sceneTickMilliseconds)
               << ",\"maintenance\":" << finite(record.maintenanceMilliseconds)
               << ",\"fixedTick\":" << finite(record.fixedTickMilliseconds)
               << ",\"sleep\":" << finite(record.sleepMilliseconds)
               << ",\"fixedSteps\":" << record.fixedSteps;
        if (record.worldStreaming.has_value()) {
            const WorldStreamingRecord& streaming = *record.worldStreaming;
            output << ",\"worldStreaming\":{\"queueDepth\":"
                   << streaming.queueDepth
                   << ",\"reading\":" << streaming.reading
                   << ",\"prepared\":" << streaming.prepared
                   << ",\"active\":" << streaming.active
                   << ",\"dormant\":" << streaming.dormant
                   << ",\"cacheBytes\":" << streaming.cacheBytes
                   << ",\"publishMilliseconds\":"
                   << finite(streaming.publishMilliseconds)
                   << ",\"visibleTileChunks\":" << streaming.visibleTileChunks
                   << ",\"activeActors\":" << streaming.activeActors << '}';
        }
        output << '}';
    }
    output << "],\"droppedLogicTicks\":" << batch.droppedLogicTicks << "}\n";
    const std::string message = output.str();
    const std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_.load(std::memory_order_relaxed) ||
        batch.generation != generation_.load(std::memory_order_acquire)) {
        return;
    }
    std::cout.write(message.data(),
                    static_cast<std::streamsize>(message.size()));
    std::cout.flush();
}

double PerformanceProfiler::milliseconds(
    std::chrono::steady_clock::duration duration) noexcept {
    return std::chrono::duration<double, std::milli>(duration).count();
}

double PerformanceProfiler::seconds(
    std::chrono::steady_clock::duration duration) noexcept {
    return std::chrono::duration<double>(duration).count();
}
