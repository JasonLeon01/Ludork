#pragma once

#include <CoreMinimal.hpp>
#include <System/SceneRuntime.hpp>

BIND_CLASS()
class SceneManager {
public:
    static void bindSceneOperationThread();

    static void applyPendingSceneReplace();

    BIND_METHOD(Pure = true)
    static std::shared_ptr<SceneRuntime> getScene();

    BIND_METHOD(Pure = true)
    static std::shared_ptr<SceneRuntime> requireScene();

    BIND_METHOD(Pure = true)
    static std::vector<std::shared_ptr<SceneRuntime>> getSceneList();

    BIND_METHOD()
    static void setScene(const std::shared_ptr<SceneRuntime>& scene);

    BIND_METHOD()
    static void pushScene(const std::shared_ptr<SceneRuntime>& scene);

    BIND_METHOD()
    static void popScene();

private:
    friend class SceneBase;
    static bool hasPendingSceneOperations();
    static void drainRetiredScenes();
};
