#pragma once

#include <Graphics/Materials/MaterialCacheRequest.hpp>
#include <Runtime/RuntimeData.hpp>

#include <atomic>
#include <exception>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <utility>

namespace ludork::engine::materials {

struct MaterialCacheRequest::Impl {
    class DirectoryLock {
    public:
        DirectoryLock(const std::filesystem::path& path,
                      const std::atomic<bool>& stop);
        ~DirectoryLock();
        DirectoryLock(const DirectoryLock&) = delete;
        DirectoryLock& operator=(const DirectoryLock&) = delete;

    private:
        std::intptr_t handle = -1;
    };

    struct Inputs {
        std::map<std::string, RuntimeData> materials;
        std::set<std::pair<std::string, std::string>> programs;
        std::map<std::string, std::string> hashes;
        bool encryptShaders = false;
        bool encryptData = false;

        RuntimeData identity() const;
    };

    explicit Impl(std::filesystem::path root);
    ~Impl();
    void run(const std::atomic<bool>& stop) noexcept;
    void prepare(const std::atomic<bool>& stop);
    Inputs captureInputs(const std::atomic<bool>& stop) const;
    bool reusable(const Inputs& inputs, const std::atomic<bool>& stop) const;
    void generate(const Inputs& inputs, const std::atomic<bool>& stop);
    void checkCancelled(const std::atomic<bool>& stop) const;

    std::filesystem::path userDataRoot;
    std::filesystem::path cacheRoot;
    mutable std::mutex mutex;
    std::string status = "pending";
    std::exception_ptr failure;
    std::atomic<double> progress = 0.0;
    std::atomic<bool> stopRequested = false;
    std::thread worker;
};

}  // namespace ludork::engine::materials
