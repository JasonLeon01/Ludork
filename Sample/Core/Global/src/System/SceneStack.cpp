#include <System.hpp>

#include "SystemRuntimeAccess.hpp"

#include <GlobalRuntimeApi.hpp>

#include <exception>
#include <stdexcept>
#include <thread>
#include <utility>

using namespace ludork::global::system_runtime;

std::shared_ptr<SceneRuntime> System::getScene() {
    const std::lock_guard<std::mutex> lock(sceneMutex_);
    return scenes_.empty() ? nullptr : scenes_.back();
}

std::shared_ptr<SceneRuntime> System::requireScene() {
    const std::shared_ptr<SceneRuntime> scene = getScene();
    if (scene == nullptr) {
        throw std::runtime_error("No active scene");
    }
    return scene;
}

std::vector<std::shared_ptr<SceneRuntime>> System::getSceneList() {
    const std::lock_guard<std::mutex> lock(sceneMutex_);
    return scenes_;
}

void System::bindSceneOperationThread() {
    const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
    if (shuttingDown_.load()) {
        return;
    }
    sceneOperationThread_ = std::this_thread::get_id();
}

void System::unbindSceneOperationThread() {
    const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
    if (sceneOperationThread_ == std::this_thread::get_id()) {
        sceneOperationThread_ = {};
    }
}

bool System::hasPendingSceneOperations() {
    const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
    return !pendingSceneOperations_.empty();
}

void System::applyPendingSceneReplace() {
    std::deque<PendingSceneOperation> operations;
    {
        const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
        if (shuttingDown_.load()) {
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
    bool applyImmediately = false;
    {
        const std::lock_guard<std::mutex> lock(pendingSceneMutex_);
        if (shuttingDown_.load()) {
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
    if (shuttingDown_.load()) {
        return;
    }
    freezeTransitionBackground();
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        if (shuttingDown_.load()) {
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

void System::applyPushScene(const std::shared_ptr<SceneRuntime>& scene) {
    const std::lock_guard<std::mutex> lock(sceneMutex_);
    if (shuttingDown_.load()) {
        return;
    }
    scenes_.push_back(scene);
}

void System::applyPopScene() {
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        if (shuttingDown_.load()) {
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

void System::applyExit() {
    std::vector<std::shared_ptr<SceneRuntime>> scenes;
    {
        const std::lock_guard<std::mutex> lock(sceneMutex_);
        if (shuttingDown_.load()) {
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

void System::drainRetiredScenes() {
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
