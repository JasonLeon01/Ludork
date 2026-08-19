#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

struct MainFramePerformanceMeasurement {
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;
    double runtimeMilliseconds{};
    double sceneOperationsMilliseconds{};
    double inputMilliseconds{};
    double uiUpdateMilliseconds{};
    double renderMilliseconds{};
    double lateUpdateMilliseconds{};
    double audioMilliseconds{};
    int targetFps{};
};

struct LogicTickPerformanceMeasurement {
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point end;
    double sceneTickMilliseconds{};
    double maintenanceMilliseconds{};
    double fixedTickMilliseconds{};
    double sleepMilliseconds{};
    int fixedSteps{};
};

class PerformanceProfiler {
public:
    static bool isEnabled() noexcept;
    static void setEnabled(bool enabled);
    static void shutdown() noexcept;
    static void beginMainFrame() noexcept;
    static void addPresentWait(double milliseconds) noexcept;
    static void recordMainFrame(
        const MainFramePerformanceMeasurement& measurement);
    static void recordLogicTick(
        const LogicTickPerformanceMeasurement& measurement);

private:
    struct MainFrameRecord {
        double timeSeconds{};
        double intervalMilliseconds{};
        double activeMilliseconds{};
        double runtimeMilliseconds{};
        double sceneOperationsMilliseconds{};
        double inputMilliseconds{};
        double uiUpdateMilliseconds{};
        double renderCpuMilliseconds{};
        double presentWaitMilliseconds{};
        double lateUpdateMilliseconds{};
        double audioMilliseconds{};
    };

    struct LogicTickRecord {
        double timeSeconds{};
        double iterationMilliseconds{};
        double sceneTickMilliseconds{};
        double maintenanceMilliseconds{};
        double fixedTickMilliseconds{};
        double sleepMilliseconds{};
        int fixedSteps{};
    };

    struct Batch {
        std::uint64_t generation{};
        double fps{};
        double memoryMegabytes{};
        int targetFps{};
        std::vector<MainFrameRecord> mainFrames;
        std::vector<LogicTickRecord> logicTicks;
        std::uint64_t droppedLogicTicks{};
    };

    static void resetLocked(std::chrono::steady_clock::time_point epoch);
    static void emit(Batch batch);
    static double milliseconds(
        std::chrono::steady_clock::duration duration) noexcept;
    static double seconds(
        std::chrono::steady_clock::duration duration) noexcept;

    static constexpr std::size_t sampleFrameCount_ = 30;
    static constexpr std::size_t maximumLogicTickCount_ = 256;
    static std::atomic_bool enabled_;
    static std::atomic_uint64_t generation_;
    static std::mutex mutex_;
    static std::chrono::steady_clock::time_point epoch_;
    static std::optional<std::chrono::steady_clock::time_point>
        previousMainFrameStart_;
    static std::optional<std::chrono::steady_clock::time_point> batchStart_;
    static std::vector<MainFrameRecord> mainFrames_;
    static std::vector<LogicTickRecord> logicTicks_;
    static std::uint64_t droppedLogicTicks_;
    static thread_local double presentWaitMilliseconds_;
};
