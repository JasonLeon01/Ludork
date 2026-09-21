#include "SceneStackImpl.hpp"
#include "LifecycleImpl.hpp"
#include "TransitionImpl.hpp"
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ludork::global::system_impl {

SceneStackImpl::SceneStackImpl(const LifecycleImpl& lifecycle,
                               TransitionImpl& transition)
    : lifecycle_(lifecycle), transition_(transition) {}

std::shared_ptr<SceneRuntime> SceneStackImpl::getScene() {
    const std::lock_guard<std::mutex> lock(sceneMutex_);
    return scenes_.empty() ? nullptr : scenes_.back();
}

std::shared_ptr<SceneRuntime> SceneStackImpl::requireScene() {
    const std::shared_ptr<SceneRuntime> scene = getScene();
    if (scene == nullptr) {
        throw std::runtime_error("No active scene");
    }
    return scene;
}

std::vector<std::shared_ptr<SceneRuntime>> SceneStackImpl::getSceneList() {
    const std::lock_guard<std::mutex> lock(sceneMutex_);
    return scenes_;
}

void SceneStackImpl::bindSceneOperationThread() {
    const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
    if (lifecycle_.isShuttingDown()) {
        return;
    }
    sceneOperationThread_ = std::this_thread::get_id();
}

void SceneStackImpl::unbindSceneOperationThread() {
    const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
    if (sceneOperationThread_ == std::this_thread::get_id()) {
        sceneOperationThread_ = {};
    }
}

bool SceneStackImpl::hasPendingSceneOperations() {
    const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
    return !pendingSceneOperations_.empty();
}

void SceneStackImpl::applyPendingSceneReplace() {
    std::deque<PendingSceneOperation> operations;
    {
        const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
        if (lifecycle_.isShuttingDown()) {
            return;
        }
        if (sceneOperationThread_ != std::thread::id{} &&
            sceneOperationThread_ != std::this_thread::get_id()) {
            return;
        }
        operations.swap(pendingSceneOperations_);
    }
    while (!operations.empty()) {
        PendingSceneOperation operation = std::move(operations.front());
        operations.pop_front();
        applySceneOperation(std::move(operation));
    }
}

void SceneStackImpl::setScene(const std::shared_ptr<SceneRuntime>& scene) {
    if (scene == nullptr) {
        throw std::invalid_argument("Scene cannot be null");
    }
    requestSceneOperation(SceneOperationType::Replace, scene);
}

void SceneStackImpl::pushScene(const std::shared_ptr<SceneRuntime>& scene) {
    if (scene == nullptr) {
        throw std::invalid_argument("Scene cannot be null");
    }
    requestSceneOperation(SceneOperationType::Push, scene);
}

void SceneStackImpl::popScene() {
    requestSceneOperation(SceneOperationType::Pop);
}

void SceneStackImpl::exit() {
    requestSceneOperation(SceneOperationType::Exit);
}

void SceneStackImpl::requestSceneOperation(
    SceneOperationType type, std::shared_ptr<SceneRuntime> scene) {
    bool applyImmediately = false;
    {
        const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
        if (lifecycle_.isShuttingDown()) {
            return;
        }
        applyImmediately = sceneOperationThread_ == std::thread::id{};
        if (!applyImmediately) {
            pendingSceneOperations_.push_back({type, std::move(scene)});
        }
    }
    if (applyImmediately) {
        applySceneOperation({type, std::move(scene)});
    }
}

void SceneStackImpl::applySceneOperation(PendingSceneOperation operation) {
    switch (operation.type) {
        case SceneOperationType::Replace:
            applySetScene(operation.scene);
            break;
        case SceneOperationType::Push:
            applyPushScene(operation.scene);
            break;
        case SceneOperationType::Pop:
            applyPopScene();
            break;
        case SceneOperationType::Exit:
            applyExit();
            break;
    }
}

void SceneStackImpl::applySetScene(const std::shared_ptr<SceneRuntime>& scene) {
    if (lifecycle_.isShuttingDown()) {
        return;
    }
    transition_.freezeTransitionBackground();
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        if (lifecycle_.isShuttingDown()) {
            return;
        }
        if (scenes_.empty()) {
            scenes_.push_back(scene);
        } else {
            retiredScenes_.push_back(std::move(scenes_.back()));
            scenes_.back() = scene;
        }
    }
    drainRetiredScenes();
}

void SceneStackImpl::applyPushScene(
    const std::shared_ptr<SceneRuntime>& scene) {
    const std::lock_guard<std::mutex> lock(sceneMutex_);
    if (lifecycle_.isShuttingDown()) {
        return;
    }
    scenes_.push_back(scene);
}

void SceneStackImpl::applyPopScene() {
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        if (lifecycle_.isShuttingDown()) {
            return;
        }
        if (scenes_.empty()) {
            throw std::logic_error("Cannot pop an empty scene stack");
        }
        retiredScenes_.push_back(std::move(scenes_.back()));
        scenes_.pop_back();
    }
    drainRetiredScenes();
}

void SceneStackImpl::applyExit() {
    std::vector<std::shared_ptr<SceneRuntime>> scenes;
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        if (lifecycle_.isShuttingDown()) {
            return;
        }
        scenes.swap(scenes_);
        for (auto iterator = scenes.rbegin(); iterator != scenes.rend();
             ++iterator) {
            retiredScenes_.push_back(std::move(*iterator));
        }
    }
    drainRetiredScenes();
}

void SceneStackImpl::drainRetiredScenes() {
    std::exception_ptr failure;
    while (true) {
        std::shared_ptr<SceneRuntime> scene;
        {
            const std::lock_guard<std::mutex> lock(sceneMutex_);
            if (retiredScenes_.empty() ||
                (retiredScenes_.front() != nullptr &&
                 retiredScenes_.front()->systemIsRunning())) {
                break;
            }
            scene = std::move(retiredScenes_.front());
            retiredScenes_.pop_front();
        }
        if (scene == nullptr) {
            continue;
        }
        try {
            scene->systemDestroy();
        } catch (...) {
            if (failure == nullptr) {
                failure = std::current_exception();
            }
        }
    }
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
}

void SceneStackImpl::reset() {
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        scenes_.clear();
        retiredScenes_.clear();
    }
    {
        const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
        pendingSceneOperations_.clear();
        sceneOperationThread_ = {};
    }
}

void SceneStackImpl::shutdown() noexcept {
    std::vector<std::shared_ptr<SceneRuntime>> scenes;
    std::deque<std::shared_ptr<SceneRuntime>> retiredScenes;
    const auto shutdownScene = [](const auto& scene) noexcept {
        if (scene == nullptr) {
            return;
        }
        try {
            scene->systemDestroy();
        } catch (const std::exception& error) {
            std::cerr << "Scene shutdown callback failed: " << error.what()
                      << '\n';
        } catch (...) {
            std::cerr
                << "Scene shutdown callback failed with an unknown error\n";
        }
        scene->systemShutdown();
    };
    {
        const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
        pendingSceneOperations_.clear();
        sceneOperationThread_ = {};
    }
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        scenes.swap(scenes_);
        retiredScenes.swap(retiredScenes_);
    }
    for (const std::shared_ptr<SceneRuntime>& scene : retiredScenes) {
        shutdownScene(scene);
    }
    retiredScenes.clear();
    for (auto iterator = scenes.rbegin(); iterator != scenes.rend();
         ++iterator) {
        shutdownScene(*iterator);
    }
    scenes.clear();
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        scenes.swap(scenes_);
        retiredScenes.swap(retiredScenes_);
    }
    for (const std::shared_ptr<SceneRuntime>& scene : retiredScenes) {
        shutdownScene(scene);
    }
    retiredScenes.clear();
    for (auto iterator = scenes.rbegin(); iterator != scenes.rend();
         ++iterator) {
        shutdownScene(*iterator);
    }
    scenes.clear();
}

SceneStackImpl& sceneStackImpl() {
    static SceneStackImpl instance(lifecycleImpl(), transitionImpl());
    return instance;
}

}  // namespace ludork::global::system_impl
