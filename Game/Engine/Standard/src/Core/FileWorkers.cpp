#include <FileWorkers.hpp>
#include <BatchWorkerPool.hpp>

#include <algorithm>
#include <bit>
#include <thread>
#include <vector>

#ifndef __EMSCRIPTEN__
#include "BatchWorkerScope.hpp"
#include <optional>
#include <utility>
#endif

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sched.h>
#include <filesystem>
#include <fstream>
#include <string>
#elif defined(__APPLE__)
#include <unistd.h>
#endif

namespace ludork::standard {
namespace {

#ifndef __EMSCRIPTEN__
thread_local std::optional<BatchWorkerPool>* taskPool = nullptr;
#endif

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
std::size_t cpuQuota(std::size_t count) {
    std::ifstream groups("/proc/self/cgroup");
    for (std::string line; std::getline(groups, line);) {
        const auto first = line.find(':');
        const auto last = line.find(':', first + 1);
        if (first == std::string::npos || last == std::string::npos) {
            continue;
        }
        const auto controllers = line.substr(first + 1, last - first - 1);
        const bool unified = controllers.empty();
        if (!unified &&
            ("," + controllers + ",").find(",cpu,") == std::string::npos) {
            continue;
        }
        std::filesystem::path root("/sys/fs/cgroup");
        if (!unified) {
            root /= "cpu";
        }
        const auto relative = std::filesystem::path(line.substr(last + 1))
                                  .lexically_normal()
                                  .relative_path();
        for (auto path = root / relative;; path = path.parent_path()) {
            long long quota = 0, period = 0;
            if (unified) {
                std::ifstream(path / "cpu.max") >> quota >> period;
            } else {
                std::ifstream(path / "cpu.cfs_quota_us") >> quota;
                std::ifstream(path / "cpu.cfs_period_us") >> period;
            }
            if (quota > 0 && period > 0) {
                count = std::min(
                    count, static_cast<std::size_t>(1 + (quota - 1) / period));
            }
            if (path == root || !path.has_relative_path()) {
                break;
            }
        }
    }
    return count;
}
#endif

std::size_t availableProcessors() {
#if defined(__EMSCRIPTEN__)
    return 1;
#elif defined(_WIN32)
    const auto system =
        std::max<DWORD>(1, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    std::size_t count = system;
    DWORD_PTR processMask = 0, systemMask = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &processMask,
                               &systemMask) &&
        processMask) {
        count = std::popcount(processMask);
    }
    USHORT groupCount = GetActiveProcessorGroupCount();
    if (groupCount > 1) {
        std::vector<USHORT> groups(groupCount);
        if (GetProcessGroupAffinity(GetCurrentProcess(), &groupCount,
                                    groups.data()) &&
            groupCount > 1) {
            count = 0;
            for (USHORT index = 0; index < groupCount; ++index) {
                count += GetActiveProcessorCount(groups[index]);
            }
            count = std::max<std::size_t>(1, count);
        }
    }
    JOBOBJECT_CPU_RATE_CONTROL_INFORMATION rate{};
    if (QueryInformationJobObject(nullptr, JobObjectCpuRateControlInformation,
                                  &rate, sizeof(rate), nullptr) &&
        (rate.ControlFlags & JOB_OBJECT_CPU_RATE_CONTROL_ENABLE)) {
        const auto cap =
            (rate.ControlFlags & JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP)
                ? rate.CpuRate
            : (rate.ControlFlags & JOB_OBJECT_CPU_RATE_CONTROL_MIN_MAX_RATE)
                ? rate.MaxRate
                : 0;
        if (cap > 0) {
            count = std::min(
                count,
                static_cast<std::size_t>(
                    (static_cast<unsigned long long>(system) * cap + 9999) /
                    10000));
        }
    }
    return count;
#elif defined(__linux__)
    cpu_set_t mask;
    CPU_ZERO(&mask);
    const auto count =
        sched_getaffinity(0, sizeof(mask), &mask) == 0 ? CPU_COUNT(&mask) : 0;
    return cpuQuota(
        count > 0 ? count : std::max(1u, std::thread::hardware_concurrency()));
#elif defined(__APPLE__)
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? static_cast<std::size_t>(count) : 1;
#else
    return std::max(1u, std::thread::hardware_concurrency());
#endif
}

}  // namespace

std::size_t fileWorkerCount() {
    static const std::size_t count = availableProcessors();
    return count;
}

#ifndef __EMSCRIPTEN__
void runWithBatchWorkerScope(const std::function<void()>& work) {
    struct Scope {
        std::optional<BatchWorkerPool> workers;
        std::optional<BatchWorkerPool>* previous =
            std::exchange(taskPool, &workers);
        ~Scope() {
            taskPool = previous;
        }
    } scope;
    work();
}
#endif

void parallelFiles(std::size_t count,
                   const std::function<void(std::size_t)>& work) {
#ifndef __EMSCRIPTEN__
    if (count == 0) {
        return;
    }
    if (taskPool) {
        if (!taskPool->has_value()) {
            taskPool->emplace();
        }
        taskPool->value().run(count, work);
        return;
    }
#endif
    BatchWorkerPool{}.run(count, work);
}

}  // namespace ludork::standard
