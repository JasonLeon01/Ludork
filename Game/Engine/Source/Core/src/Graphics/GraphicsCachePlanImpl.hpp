#pragma once
#include <Graphics/GraphicsCachePlan.hpp>
#include <BackgroundTask.hpp>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>

namespace ludork::engine {
struct GraphicsCachePlan::Impl {
    explicit Impl(const std::filesystem::path& root);
    void run(const std::filesystem::path& root) noexcept;
    void waitFor(const char* waiting, const char* running);
    void start(const char* waiting, const char* running);
    void requirePlanned() const;

    mutable std::mutex mutex;
    std::condition_variable changed;
    std::atomic<bool> stop = false;
    std::string status = "planning";
    bool materialExport = false;
    bool shaderCompilation = false;
    std::atomic<double> progress = 0;
    std::exception_ptr failure;
    ludork::standard::BackgroundTask worker;
};
}  // namespace ludork::engine
