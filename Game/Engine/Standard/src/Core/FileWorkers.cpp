#include <FileWorkers.hpp>
#include <BackgroundTask.hpp>

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

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

void parallelFiles(std::size_t count,
                   const std::function<void(std::size_t)>& work) {
    const auto workers = std::min(count, fileWorkerCount());
    if (workers <= 1) {
#if defined(__EMSCRIPTEN__)
        auto slice = std::chrono::steady_clock::now();
#endif
        for (std::size_t index = 0; index < count; ++index) {
            work(index);
#if defined(__EMSCRIPTEN__)
            if (std::chrono::steady_clock::now() - slice >=
                std::chrono::milliseconds(2)) {
                BackgroundTask::yield();
                slice = std::chrono::steady_clock::now();
            }
#endif
        }
        return;
    }
    std::atomic_size_t next{0};
    std::mutex mutex;
    std::exception_ptr failure;
    const auto run = [&] {
        try {
            for (;;) {
                const auto index = next.fetch_add(1, std::memory_order_relaxed);
                if (index >= count) {
                    return;
                }
                work(index);
            }
        } catch (...) {
            next.store(count, std::memory_order_relaxed);
            std::lock_guard lock(mutex);
            if (!failure) {
                failure = std::current_exception();
            }
        }
    };
    std::vector<BackgroundTask> threads;
    threads.reserve(workers - 1);
    for (std::size_t index = 1; index < workers; ++index) {
        threads.emplace_back(run);
    }
    run();
    threads.clear();
    if (failure) {
        std::rethrow_exception(failure);
    }
}

}  // namespace ludork::standard
