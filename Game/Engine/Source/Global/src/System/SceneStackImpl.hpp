#pragma once

#include <System/SceneRuntime.hpp>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ludork::global::system_impl {

class LifecycleImpl;
class TransitionImpl;

class SceneStackImpl {
public:
    SceneStackImpl(const LifecycleImpl& lifecycle, TransitionImpl& transition);
    std::shared_ptr<SceneRuntime> getScene();
    std::shared_ptr<SceneRuntime> requireScene();
    std::vector<std::shared_ptr<SceneRuntime>> getSceneList();
    void bindSceneOperationThread();
    void unbindSceneOperationThread();
    bool hasPendingSceneOperations();
    void applyPendingSceneReplace();
    void setScene(const std::shared_ptr<SceneRuntime>& scene);
    void pushScene(const std::shared_ptr<SceneRuntime>& scene);
    void popScene();
    void exit();
    void drainRetiredScenes();
    void reset();
    void shutdown() noexcept;

private:
    enum class SceneOperationType {
        Replace,
        Push,
        Pop,
        Exit
    };
    struct PendingSceneOperation {
        SceneOperationType type;
        std::shared_ptr<SceneRuntime> scene;
    };
    void requestSceneOperation(SceneOperationType type,
                               std::shared_ptr<SceneRuntime> scene = {});
    void applySceneOperation(PendingSceneOperation operation);
    void applySetScene(const std::shared_ptr<SceneRuntime>& scene);
    void applyPushScene(const std::shared_ptr<SceneRuntime>& scene);
    void applyPopScene();
    void applyExit();
    const LifecycleImpl& lifecycle_;
    TransitionImpl& transition_;

    std::vector<std::shared_ptr<SceneRuntime>> scenes_;
    std::deque<std::shared_ptr<SceneRuntime>> retiredScenes_;
    std::deque<PendingSceneOperation> pendingSceneOperations_;
    std::mutex sceneMutex_;
    std::mutex pendingSceneMutex_;
    std::thread::id sceneOperationThread_;
};

SceneStackImpl& sceneStackImpl();

}  // namespace ludork::global::system_impl
