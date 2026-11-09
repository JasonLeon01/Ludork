#pragma once

#include <EngineRuntimeApi.hpp>
#include <LudorkRuntimeBinding/Annotations.hpp>
#include <filesystem>
#include <memory>
#include <string>

namespace ludork::engine::materials {

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API MaterialCacheRequest {
public:
    ~MaterialCacheRequest();
    MaterialCacheRequest(const MaterialCacheRequest&) = delete;
    MaterialCacheRequest& operator=(const MaterialCacheRequest&) = delete;

    static std::shared_ptr<MaterialCacheRequest> create(
        const std::filesystem::path& userDataRoot);

    /// Return pending, ready, failed or cancelled without blocking.
    BIND_METHOD(metadata = false)
    std::string getStatus() const;

    /// Return preparation progress between zero and one.
    BIND_METHOD(metadata = false)
    double getProgress() const;

    /// Confirm successful preparation; failed requests rethrow their error.
    BIND_METHOD(metadata = false)
    void getResult() const;

    /// Cancel preparation and wait for its worker to finish.
    BIND_METHOD(metadata = false)
    void cancel();

private:
    explicit MaterialCacheRequest(const std::filesystem::path& userDataRoot);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::engine::materials
