#pragma once

#include <System/SceneRuntime.hpp>

#include <memory>
#include <vector>

namespace ludork::global::system_impl {
struct PendingSceneOperation;
enum class SceneOperationType;
}  // namespace ludork::global::system_impl

namespace ludork::global::system_scene_stack_impl {

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
void requestSceneOperation(ludork::global::system_impl::SceneOperationType type,
                           std::shared_ptr<SceneRuntime> scene = {});
void applySceneOperation(
    ludork::global::system_impl::PendingSceneOperation operation);
void applySetScene(const std::shared_ptr<SceneRuntime>& scene);
void applyPushScene(const std::shared_ptr<SceneRuntime>& scene);
void applyPopScene();
void applyExit();
void drainRetiredScenes();

}  // namespace ludork::global::system_scene_stack_impl
