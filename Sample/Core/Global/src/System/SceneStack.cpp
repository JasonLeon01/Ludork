#include <System.hpp>

#include "SystemRuntime.hpp"

#include <GlobalRuntimeApi.hpp>

#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

std::shared_ptr<SceneRuntime> System::getScene() {
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        ludork::global::system_runtime::runtime().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
    return sceneStack.scenes_.empty() ? nullptr : sceneStack.scenes_.back();
}

std::shared_ptr<SceneRuntime> System::requireScene() {
    const std::shared_ptr<SceneRuntime> scene = getScene();
    if (scene == nullptr) {
        throw std::runtime_error("No active scene");
    }
    return scene;
}

std::vector<std::shared_ptr<SceneRuntime>> System::getSceneList() {
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        ludork::global::system_runtime::runtime().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
    return sceneStack.scenes_;
}

void System::bindSceneOperationThread() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
    if (lifecycle.shuttingDown_.load()) {
        return;
    }
    sceneStack.sceneOperationThread_ = std::this_thread::get_id();
}

void System::unbindSceneOperationThread() {
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        ludork::global::system_runtime::runtime().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
    if (sceneStack.sceneOperationThread_ == std::this_thread::get_id()) {
        sceneStack.sceneOperationThread_ = {};
    }
}

bool System::hasPendingSceneOperations() {
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        ludork::global::system_runtime::runtime().sceneStack;
    const std::lock_guard<std::mutex> lock(sceneStack.pendingSceneMutex_);
    return !sceneStack.pendingSceneOperations_.empty();
}

void System::applyPendingSceneReplace() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    std::deque<PendingSceneOperation> operations;
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
        PendingSceneOperation operation = std::move(operations.front());
        operations.pop_front();
        applySceneOperation(std::move(operation));
    }
}

void System::setScene(const std::shared_ptr<SceneRuntime>& scene) {
    if (scene == nullptr) {
        throw std::invalid_argument("Scene cannot be null");
    }
    requestSceneOperation(SceneOperationType::Replace, scene);
}

void System::pushScene(const std::shared_ptr<SceneRuntime>& scene) {
    if (scene == nullptr) {
        throw std::invalid_argument("Scene cannot be null");
    }
    requestSceneOperation(SceneOperationType::Push, scene);
}

void System::popScene() {
    requestSceneOperation(SceneOperationType::Pop);
}

void System::exit() {
    requestSceneOperation(SceneOperationType::Exit);
}
void System::requestSceneOperation(SceneOperationType type,
                                   std::shared_ptr<SceneRuntime> scene) {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
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
        applySceneOperation({type, std::move(scene)});
    }
}

void System::applySceneOperation(PendingSceneOperation operation) {
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

void System::applySetScene(const std::shared_ptr<SceneRuntime>& scene) {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    if (lifecycle.shuttingDown_.load()) {
        return;
    }
    freezeTransitionBackground();
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
    drainRetiredScenes();
}

void System::applyPushScene(const std::shared_ptr<SceneRuntime>& scene) {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
    const std::lock_guard<std::mutex> lock(sceneStack.sceneMutex_);
    if (lifecycle.shuttingDown_.load()) {
        return;
    }
    sceneStack.scenes_.push_back(scene);
}

void System::applyPopScene() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
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
    drainRetiredScenes();
}

void System::applyExit() {
    ludork::global::system_runtime::SystemRuntime& systemRuntime =
        ludork::global::system_runtime::runtime();
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        systemRuntime.sceneStack;
    ludork::global::system_runtime::LifecycleRuntime& lifecycle =
        systemRuntime.lifecycle;
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
    drainRetiredScenes();
}

void System::drainRetiredScenes() {
    ludork::global::system_runtime::SceneStackRuntime& sceneStack =
        ludork::global::system_runtime::runtime().sceneStack;
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
