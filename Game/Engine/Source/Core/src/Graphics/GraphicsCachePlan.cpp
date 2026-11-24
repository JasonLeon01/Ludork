#include <Graphics/GraphicsCachePlan.hpp>
#include "GraphicsCachePlanImpl.hpp"
#include "Materials/MaterialCachePreparation.hpp"
#include "Shaders/ShaderCachePreparation.hpp"
#include <Runtime/Graphics/ShaderCache.hpp>
#include <PersistentStorage.hpp>
#include <stdexcept>

namespace ludork::engine {
GraphicsCachePlan::GraphicsCachePlan(const std::filesystem::path& root)
    : impl_(std::make_unique<Impl>(root)) {}

GraphicsCachePlan::~GraphicsCachePlan() {
    cancel();
}

std::string GraphicsCachePlan::getStatus() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->status;
}

void GraphicsCachePlan::Impl::requirePlanned() const {
    if (failure) {
        std::rethrow_exception(failure);
    }
    if (status == "planning" || status == "cancelled") {
        throw std::logic_error("Graphics cache plan is " + status);
    }
}

bool GraphicsCachePlan::needsMaterialExport() const {
    std::lock_guard lock(impl_->mutex);
    impl_->requirePlanned();
    return impl_->materialExport;
}

bool GraphicsCachePlan::needsShaderCompilation() const {
    std::lock_guard lock(impl_->mutex);
    impl_->requirePlanned();
    return impl_->shaderCompilation;
}

void GraphicsCachePlan::startMaterials() {
    impl_->start("planned", "materials");
}
void GraphicsCachePlan::startShaders() {
    impl_->start("materialsReady", "shaders");
}

double GraphicsCachePlan::getProgress() const {
    return impl_->progress.load();
}

void GraphicsCachePlan::getResult() const {
    std::lock_guard lock(impl_->mutex);
    if (impl_->failure) {
        std::rethrow_exception(impl_->failure);
    }
    if (impl_->status != "ready") {
        throw std::logic_error("Graphics cache plan is " + impl_->status);
    }
}

void GraphicsCachePlan::cancel() {
    {
        std::lock_guard lock(impl_->mutex);
        impl_->stop = true;
    }
    impl_->changed.notify_all();
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
}

GraphicsCachePlan::Impl::Impl(const std::filesystem::path& root)
    : worker([this, root] {
          run(root);
      }) {}

void GraphicsCachePlan::Impl::start(const char* waiting, const char* running) {
    {
        std::lock_guard lock(mutex);
        if (failure) {
            std::rethrow_exception(failure);
        }
        if (status != waiting) {
            throw std::logic_error("Graphics cache plan is " + status);
        }
        progress = 0.0;
        status = running;
    }
    changed.notify_all();
}

void GraphicsCachePlan::Impl::waitFor(const char* waiting,
                                      const char* running) {
    std::unique_lock lock(mutex);
    status = waiting;
    ludork::standard::BackgroundTask::wait(changed, lock, [&] {
        return stop.load() || status == running;
    });
    if (stop.load()) {
        throw std::runtime_error("Graphics cache planning was cancelled");
    }
}

void GraphicsCachePlan::Impl::run(const std::filesystem::path& root) noexcept {
    using ludork::runtime::graphics::ShaderCache;
    try {
        ShaderCache::withPreparationLock(stop, [&] {
            materials::MaterialCachePreparation::prepare(
                root, stop,
                [&](double value) {
                    progress = value;
                },
                [&](const ShaderCache::MaterialProjection* projectedMaterials,
                    const std::function<void()>& publishMaterials) {
                    shaders::prepareShaderPrograms(
                        root, {}, stop,
                        [&](double value) {
                            progress = value;
                        },
                        projectedMaterials,
                        [&](bool compile) {
                            {
                                std::lock_guard lock(mutex);
                                materialExport = projectedMaterials != nullptr;
                                shaderCompilation = compile;
                            }
                            waitFor("planned", "materials");
                            publishMaterials();
                            if (materialExport) {
                                ludork::standard::persistStorage(root);
                            }
                            progress = 1.0;
                            waitFor("materialsReady", "shaders");
                        });
                });
        });
        ShaderCache::savePipelineCache();
        std::lock_guard lock(mutex);
        progress = 1.0;
        status = "ready";
    } catch (...) {
        std::lock_guard lock(mutex);
        if (stop.load()) {
            status = "cancelled";
        } else {
            failure = std::current_exception();
            status = "failed";
        }
    }
}
}  // namespace ludork::engine
