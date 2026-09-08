#pragma once

#include "DisplayImpl.hpp"
#include "FramePipelineImpl.hpp"
#include "LifecycleImpl.hpp"
#include "SceneStackImpl.hpp"
#include <ConfigParser.hpp>

namespace ludork::global::system_impl {

class SystemImpl {
public:
    void addGraphicsShader(const std::shared_ptr<sf::Shader>& shader,
                           std::optional<ShaderUniforms> uniforms);
    void applyPendingSceneReplace();
    void applyPendingTransition();
    void applyScreenTonePass();
    void bindSceneOperationThread();
    void cancelPendingTransition();
    void cancelTransitionBackgroundFreeze();
    void changeScreenTone(float red, float green, float blue, float gray,
                          float duration);
    void clearScreenTone(float duration);
    void drainRetiredScenes();
    void draw(const sf::Drawable& drawable, sf::Shader* shader);
    void exit();
    void flashScreen(std::optional<sf::Color> color, float duration);
    void freezeTransitionBackground();
    sf::RenderTexture* getCanvas();
    sf::Vector2u getGameSize();
    std::optional<float> getMaximumWindowedScale(const sf::Vector2u& gameSize);
    std::shared_ptr<SceneRuntime> getScene();
    std::vector<std::shared_ptr<SceneRuntime>> getSceneList();
    std::shared_ptr<sf::RenderWindow> getWindow();
    bool hasPendingSceneOperations();
    void initCanvas(const sf::Vector2u& size);
    void initWindow(const std::shared_ptr<sf::RenderWindow>& window);
    void initializeRuntimeSession() noexcept;
    bool isDebugMode();
    bool isDisplayScaleConfigurable();
    bool isFlashing();
    bool isInTransition();
    bool isScreenToneActive();
    bool isScreenToneTransitionComplete();
    bool isShaking();
    bool isTransitionBackgroundFreezePending();
    bool isTransitionBackgroundFrozen();
    bool isTransitionPending();
    void popScene();
    void pushScene(const std::shared_ptr<SceneRuntime>& scene);
    void removeAllGraphicsShaders();
    void removeGraphicsShader(const std::shared_ptr<sf::Shader>& shader);
    void removeGraphicsShaderAt(int index);
    void requestTransition(std::optional<std::string> transitionName,
                           float transitionTime);
    std::shared_ptr<SceneRuntime> requireScene();
    void setDebugMode(bool debugMode);
    void setGameSize(const sf::Vector2u& gameSize);
    void setInputMethodDisabled(bool disabled);
    void setScene(const std::shared_ptr<SceneRuntime>& scene);
    void setStandardUpdate(std::function<void()> update);
    void setTransition(const std::shared_ptr<sf::Texture>& transitionResource,
                       float transitionTime);
    void setWindowDefaultView();
    void setWindowMapView(const sf::IntRect& rect);
    void startShake(float power, float speed, float duration);
    void stopFlash();
    void stopScreenTone();
    void stopShake();
    void updateRuntime();
    void init(const std::shared_ptr<ludork::standard::ConfigParser>& data,
              const std::string& dataFilePath);
    bool isActive();
    bool shouldLoop();
    void run();
    void shutdownRuntime() noexcept;
    void initializeDisplay(const std::string& title,
                           const sf::Vector2u& gameSize,
                           const std::string& iconPath,
                           const std::string& cursorPath);
    void clearCanvas();
    void composeFrame(float deltaTime);
    void present();
    void completeFrame();

private:
    void rebuildDisplayTargets(float surfaceFitScale);
    void applyPendingDisplayChanges();
    void onConfigChanged(const std::string& key);
    LifecycleImpl lifecycle_;
    DisplayImpl display_;
    FramePipelineImpl framePipeline_;
    SceneStackImpl sceneStack_{lifecycle_, framePipeline_};
};

SystemImpl& impl();

}  // namespace ludork::global::system_impl
