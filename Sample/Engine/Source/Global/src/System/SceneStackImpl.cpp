#include "SceneStackImpl.hpp"
#include "SystemImpl.hpp"
#include "TransitionImpl.hpp"
#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

namespace ludork::global::system_scene_stack_impl {

std::shared_ptr<SceneRuntime> getScene() {
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        ludork::global::system_impl::impl().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
    return sceneStack.scenes_.empty() ? nullptr : sceneStack.scenes_.back();
}

std::shared_ptr<SceneRuntime> requireScene() {
    const std::shared_ptr<SceneRuntime> scene =
        ludork::global::system_scene_stack_impl::getScene();
    if (scene == nullptr) {
        throw std::runtime_error("No active scene");
    }
    return scene;
}

std::vector<std::shared_ptr<SceneRuntime>> getSceneList() {
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        ludork::global::system_impl::impl().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
    return sceneStack.scenes_;
}

void bindSceneOperationThread() {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        systemImpl.sceneStack;
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        systemImpl.lifecycle;
    const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
    if (lifecycle.shuttingDown_.load()) {
        return;
    }
    sceneStack.sceneOperationThread_ = std::this_thread::get_id();
}

void unbindSceneOperationThread() {
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        ludork::global::system_impl::impl().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
    if (sceneStack.sceneOperationThread_ == std::this_thread::get_id()) {
        sceneStack.sceneOperationThread_ = {};
    }
}

bool hasPendingSceneOperations() {
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        ludork::global::system_impl::impl().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
    return !sceneStack.pendingSceneOperations_.empty();
}

void applyPendingSceneReplace() {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        systemImpl.sceneStack;
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        systemImpl.lifecycle;
    std::deque<ludork::global::system_impl::PendingSceneOperation> operations;
    {
        const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
        if (lifecycle.shuttingDown_.load()) {
            return;
        }
        if (sceneStack.sceneOperationThread_ != std::thread::id{} &&
            sceneStack.sceneOperationThread_ != std::this_thread::get_id()) {
            return;
        }
        operations.swap(sceneStack.pendingSceneOperations_);
    }
    while (!operations.empty()) {
        ludork::global::system_impl::PendingSceneOperation operation =
            std::move(operations.front());
        operations.pop_front();
        ludork::global::system_scene_stack_impl::applySceneOperation(
            std::move(operation));
    }
}

void setScene(const std::shared_ptr<SceneRuntime>& scene) {
    if (scene == nullptr) {
        throw std::invalid_argument("Scene cannot be null");
    }
    ludork::global::system_scene_stack_impl::requestSceneOperation(
        ludork::global::system_impl::SceneOperationType::Replace, scene);
}

void pushScene(const std::shared_ptr<SceneRuntime>& scene) {
    if (scene == nullptr) {
        throw std::invalid_argument("Scene cannot be null");
    }
    ludork::global::system_scene_stack_impl::requestSceneOperation(
        ludork::global::system_impl::SceneOperationType::Push, scene);
}

void popScene() {
    ludork::global::system_scene_stack_impl::requestSceneOperation(
        ludork::global::system_impl::SceneOperationType::Pop);
}

void exit() {
    ludork::global::system_scene_stack_impl::requestSceneOperation(
        ludork::global::system_impl::SceneOperationType::Exit);
}

void requestSceneOperation(ludork::global::system_impl::SceneOperationType type,
                           std::shared_ptr<SceneRuntime> scene) {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        systemImpl.sceneStack;
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        systemImpl.lifecycle;
    bool applyImmediately = false;
    {
        const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
        if (lifecycle.shuttingDown_.load()) {
            return;
        }
        applyImmediately =
            sceneStack.sceneOperationThread_ == std::thread::id{};
        if (!applyImmediately) {
            sceneStack.pendingSceneOperations_.push_back(
                {type, std::move(scene)});
        }
    }
    if (applyImmediately) {
        ludork::global::system_scene_stack_impl::applySceneOperation(
            {type, std::move(scene)});
    }
}

void applySceneOperation(
    ludork::global::system_impl::PendingSceneOperation operation) {
    switch (operation.type) {
        case ludork::global::system_impl::SceneOperationType::Replace:
            ludork::global::system_scene_stack_impl::applySetScene(
                operation.scene);
            break;
        case ludork::global::system_impl::SceneOperationType::Push:
            ludork::global::system_scene_stack_impl::applyPushScene(
                operation.scene);
            break;
        case ludork::global::system_impl::SceneOperationType::Pop:
            ludork::global::system_scene_stack_impl::applyPopScene();
            break;
        case ludork::global::system_impl::SceneOperationType::Exit:
            ludork::global::system_scene_stack_impl::applyExit();
            break;
    }
}

void applySetScene(const std::shared_ptr<SceneRuntime>& scene) {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        systemImpl.sceneStack;
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        systemImpl.lifecycle;
    if (lifecycle.shuttingDown_.load()) {
        return;
    }
    ludork::global::system_transition_impl::freezeTransitionBackground();
    {
        const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
        if (lifecycle.shuttingDown_.load()) {
            return;
        }
        if (sceneStack.scenes_.empty()) {
            sceneStack.scenes_.push_back(scene);
        } else {
            sceneStack.retiredScenes_.push_back(
                std::move(sceneStack.scenes_.back()));
            sceneStack.scenes_.back() = scene;
        }
    }
    ludork::global::system_scene_stack_impl::drainRetiredScenes();
}

void applyPushScene(const std::shared_ptr<SceneRuntime>& scene) {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        systemImpl.sceneStack;
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        systemImpl.lifecycle;
    const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
    if (lifecycle.shuttingDown_.load()) {
        return;
    }
    sceneStack.scenes_.push_back(scene);
}

void applyPopScene() {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        systemImpl.sceneStack;
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        systemImpl.lifecycle;
    {
        const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
        if (lifecycle.shuttingDown_.load()) {
            return;
        }
        if (sceneStack.scenes_.empty()) {
            throw std::logic_error("Cannot pop an empty scene stack");
        }
        sceneStack.retiredScenes_.push_back(
            std::move(sceneStack.scenes_.back()));
        sceneStack.scenes_.pop_back();
    }
    ludork::global::system_scene_stack_impl::drainRetiredScenes();
}

void applyExit() {
    ludork::global::system_impl::SystemImpl& systemImpl =
        ludork::global::system_impl::impl();
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        systemImpl.sceneStack;
    ludork::global::system_impl::LifecycleImpl& lifecycle =
        systemImpl.lifecycle;
    std::vector<std::shared_ptr<SceneRuntime>> scenes;
    {
        const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
        if (lifecycle.shuttingDown_.load()) {
            return;
        }
        scenes.swap(sceneStack.scenes_);
        for (auto iterator = scenes.rbegin(); iterator != scenes.rend();
             ++iterator) {
            sceneStack.retiredScenes_.push_back(std::move(*iterator));
        }
    }
    ludork::global::system_scene_stack_impl::drainRetiredScenes();
}

void drainRetiredScenes() {
    ludork::global::system_impl::SceneStackImpl& sceneStack =
        ludork::global::system_impl::impl().sceneStack;
    std::exception_ptr failure;
    while (true) {
        std::shared_ptr<SceneRuntime> scene;
        {
            const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
            if (sceneStack.retiredScenes_.empty() ||
                (sceneStack.retiredScenes_.front() != nullptr &&
                 sceneStack.retiredScenes_.front()->systemIsRunning())) {
                break;
            }
            scene = std::move(sceneStack.retiredScenes_.front());
            sceneStack.retiredScenes_.pop_front();
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

}  // namespace ludork::global::system_scene_stack_impl
