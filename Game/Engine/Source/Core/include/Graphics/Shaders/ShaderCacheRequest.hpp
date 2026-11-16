#pragma once

#include <EngineRuntimeApi.hpp>
#include <LudorkRuntimeBinding/Annotations.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ludork::engine::shaders {

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API ShaderCacheRequest {
public:
    ~ShaderCacheRequest();
    ShaderCacheRequest(const ShaderCacheRequest&) = delete;
    ShaderCacheRequest& operator=(const ShaderCacheRequest&) = delete;

    static std::shared_ptr<ShaderCacheRequest> create(
        const std::filesystem::path& userDataRoot,
        const std::vector<std::vector<std::string>>& additionalPrograms = {});

    /// Return pending, ready, failed or cancelled without blocking.
    BIND_METHOD(metadata = false)
    std::string getStatus() const;

    /// Return shader preparation progress between zero and one.
    BIND_METHOD(metadata = false)
    double getProgress() const;

    /// Confirm completion or propagate the preparation error.
    BIND_METHOD(metadata = false)
    void getResult() const;

    /// Cancel preparation and wait for the worker to finish.
    BIND_METHOD(metadata = false)
    void cancel();

private:
    ShaderCacheRequest(
        const std::filesystem::path& userDataRoot,
        const std::vector<std::vector<std::string>>& additionalPrograms);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::engine::shaders
