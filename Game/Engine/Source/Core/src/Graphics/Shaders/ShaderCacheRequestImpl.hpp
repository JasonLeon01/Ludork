#pragma once

#include <Graphics/Shaders/ShaderCacheRequest.hpp>

#include <atomic>
#include <exception>
#include <mutex>
#include <thread>

namespace ludork::engine::shaders {

struct ShaderCacheRequest::Impl {
    Impl(std::filesystem::path root,
         std::vector<std::vector<std::string>> additional);
    ~Impl();
    void run() noexcept;
    void prepare();

    std::filesystem::path userDataRoot;
    std::vector<std::vector<std::string>> additionalPrograms;
    mutable std::mutex mutex;
    std::string status = "pending";
    std::exception_ptr failure;
    std::atomic<double> progress = 0.0;
    std::atomic<bool> stop = false;
    std::thread worker;
};

}  // namespace ludork::engine::shaders
