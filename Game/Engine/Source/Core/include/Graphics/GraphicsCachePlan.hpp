#pragma once

#include <EngineRuntimeApi.hpp>
#include <LudorkRuntimeBinding/Annotations.hpp>
#include <filesystem>
#include <memory>
#include <string>

namespace ludork::engine {

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API GraphicsCachePlan {
public:
    explicit GraphicsCachePlan(const std::filesystem::path& root);
    ~GraphicsCachePlan();
    GraphicsCachePlan(const GraphicsCachePlan&) = delete;
    GraphicsCachePlan& operator=(const GraphicsCachePlan&) = delete;

    /// Return planning, planned, materials, materialsReady, shaders, ready,
    /// failed or cancelled.
    BIND_METHOD(metadata = false)
    std::string getStatus() const;

    /// Report whether the planned material cache must be exported.
    BIND_METHOD(metadata = false)
    bool needsMaterialExport() const;

    /// Report whether any planned shader requires translation or compilation.
    BIND_METHOD(metadata = false)
    bool needsShaderCompilation() const;

    /// Publish the planned material outputs when the request is planned.
    BIND_METHOD(metadata = false)
    void startMaterials();

    /// Restore or compile the planned shader programs after materials are
    /// ready.
    BIND_METHOD(metadata = false)
    void startShaders();

    BIND_METHOD(metadata = false)
    double getProgress() const;
    BIND_METHOD(metadata = false)
    void getResult() const;
    BIND_METHOD(metadata = false)
    void cancel();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace ludork::engine
