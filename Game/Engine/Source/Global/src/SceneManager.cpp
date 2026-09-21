#include <SceneManager.hpp>
#include "System/SceneStackImpl.hpp"
#include <utility>

std::shared_ptr<SceneRuntime> SceneManager::getScene() {
    return ludork::global::system_impl::sceneStackImpl().getScene();
}

std::shared_ptr<SceneRuntime> SceneManager::requireScene() {
    return ludork::global::system_impl::sceneStackImpl().requireScene();
}

std::vector<std::shared_ptr<SceneRuntime>> SceneManager::getSceneList() {
    return ludork::global::system_impl::sceneStackImpl().getSceneList();
}

void SceneManager::bindSceneOperationThread() {
    ludork::global::system_impl::sceneStackImpl().bindSceneOperationThread();
}

bool SceneManager::hasPendingSceneOperations() {
    return ludork::global::system_impl::sceneStackImpl()
        .hasPendingSceneOperations();
}

void SceneManager::applyPendingSceneReplace() {
    ludork::global::system_impl::sceneStackImpl().applyPendingSceneReplace();
}

void SceneManager::setScene(const std::shared_ptr<SceneRuntime>& scene) {
    ludork::global::system_impl::sceneStackImpl().setScene(scene);
}

void SceneManager::pushScene(const std::shared_ptr<SceneRuntime>& scene) {
    ludork::global::system_impl::sceneStackImpl().pushScene(scene);
}

void SceneManager::popScene() {
    ludork::global::system_impl::sceneStackImpl().popScene();
}

void SceneManager::drainRetiredScenes() {
    ludork::global::system_impl::sceneStackImpl().drainRetiredScenes();
}
