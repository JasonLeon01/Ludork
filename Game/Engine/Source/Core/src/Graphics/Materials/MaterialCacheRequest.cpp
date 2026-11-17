#include <Graphics/Materials/MaterialCacheRequest.hpp>
#include "MaterialCacheRequestImpl.hpp"
#include <PersistentStorage.hpp>

#include <chrono>
#include <stdexcept>

namespace ludork::engine::materials {

MaterialCacheRequest::MaterialCacheRequest(
    const std::filesystem::path& userDataRoot)
    : impl_(std::make_unique<Impl>(userDataRoot)) {}

MaterialCacheRequest::~MaterialCacheRequest() = default;

std::shared_ptr<MaterialCacheRequest> MaterialCacheRequest::create(
    const std::filesystem::path& userDataRoot) {
    return std::shared_ptr<MaterialCacheRequest>(
        new MaterialCacheRequest(userDataRoot));
}

std::string MaterialCacheRequest::getStatus() const {
    std::lock_guard lock(impl_->mutex);
    return impl_->status;
}

double MaterialCacheRequest::getProgress() const {
    return impl_->progress.load();
}

void MaterialCacheRequest::getResult() const {
    std::lock_guard lock(impl_->mutex);
    if (impl_->failure) {
        std::rethrow_exception(impl_->failure);
    }
    if (impl_->status != "ready") {
        throw std::logic_error("Material cache is " + impl_->status);
    }
}

void MaterialCacheRequest::cancel() {
    impl_->stopRequested.store(true);
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
}

MaterialCacheRequest::Impl::Impl(std::filesystem::path root)
    : userDataRoot(std::move(root)),
      cacheRoot(userDataRoot / "Assets" / "Shaders" / "MaterialGenerated"),
      worker([this] {
          run(stopRequested);
      }) {}

MaterialCacheRequest::Impl::~Impl() {
    stopRequested.store(true);
    if (worker.joinable()) {
        worker.join();
    }
}

void MaterialCacheRequest::Impl::checkCancelled(
    const std::atomic<bool>& stop) const {
#if defined(__EMSCRIPTEN__)
    if (std::chrono::steady_clock::now() - workSliceStart >=
        std::chrono::milliseconds(2)) {
        ludork::standard::BackgroundTask::yield();
        workSliceStart = std::chrono::steady_clock::now();
    }
#endif
    if (stop.load()) {
        throw std::runtime_error("Material cache preparation was cancelled");
    }
}

void MaterialCacheRequest::Impl::run(const std::atomic<bool>& stop) noexcept {
    try {
#if defined(__EMSCRIPTEN__)
        workSliceStart = std::chrono::steady_clock::now();
#endif
        prepare(stop);
        ludork::standard::persistStorage(userDataRoot);
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

}  // namespace ludork::engine::materials
