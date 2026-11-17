#pragma once

#include <Graphics/Materials/MaterialCacheRequest.hpp>
#include <Runtime/RuntimeData.hpp>

#include <atomic>
#include <chrono>
#include <exception>
#include <map>
#include <mutex>
#include <set>
#include <BackgroundTask.hpp>
#include <utility>

namespace ludork::engine::materials {

struct MaterialCacheRequest::Impl {
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
    void preserveShaderArtifacts(
        const std::filesystem::path& staging,
        const std::map<std::string, std::string>& sources, bool encryptedRecord,
        const std::atomic<bool>& stop) const;
    void checkCancelled(const std::atomic<bool>& stop) const;

    std::filesystem::path userDataRoot;
    std::filesystem::path cacheRoot;
    mutable std::mutex mutex;
    std::string status = "pending";
    std::exception_ptr failure;
    std::atomic<double> progress = 0.0;
    std::atomic<bool> stopRequested = false;
#if defined(__EMSCRIPTEN__)
    mutable std::chrono::steady_clock::time_point workSliceStart;
#endif
    ludork::standard::BackgroundTask worker;
};

}  // namespace ludork::engine::materials
