#pragma once

#include <System.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace ludork::global::system_runtime {

struct DisplayRuntime {
    std::shared_ptr<sf::RenderWindow> window_;
    std::mutex windowMutex_;
    std::unique_ptr<sf::Cursor> cursor_;
    std::string windowTitle_;
    std::string windowIconPath_;
    std::string windowCursorPath_;
    sf::ContextSettings windowContextSettings_;
    sf::Vector2u observedWindowSize_;
    std::optional<sf::Vector2u> observedWindowClientSize_;
    std::optional<float> pendingConfiguredScale_;
    std::optional<float> pendingResizeScale_;
    std::chrono::steady_clock::time_point lastResizeTime_;
    bool desktopFullscreen_ = false;
    bool inputMethodDisabled_ = true;
    bool canvasDefaultViewActive_ = true;
    std::unique_ptr<sf::RenderTexture> canvas_;
    std::optional<sf::Sprite> canvasSprite_;
};

struct FramePipelineRuntime {
    std::unique_ptr<sf::RenderTexture> transition_;
    std::unique_ptr<sf::RenderTexture> transitionTempTexture_;
    std::unique_ptr<sf::RenderTexture> transitionOutputTexture_;
    std::unique_ptr<sf::RenderTexture> transitionMaskTexture_;
    std::optional<sf::Sprite> transitionSprite_;
    std::optional<sf::Sprite> transitionOutputSprite_;
    std::vector<std::unique_ptr<sf::RenderTexture>> graphicsCanvases_;
    std::vector<std::shared_ptr<sf::Shader>> graphicsShaders_;
    std::shared_ptr<sf::Shader> transitionShader_;
    std::shared_ptr<sf::Texture> transitionResource_;
    bool inTransition_ = false;
    float transitionTimeCount_ = 0.0f;
    float transitionTime_ = 0.0f;
    std::size_t transitionRevision_ = 0;
    std::size_t composedTransitionRevision_ = 0;
    bool transitionCompletionPending_ = false;
    bool transitionFrozen_ = false;
    bool transitionFreezePending_ = false;
    std::optional<System::PendingTransition> pendingTransition_;
    std::mutex transitionMutex_;
    std::mutex presentMutex_;

    std::shared_ptr<sf::Shader> flashShader_;
    sf::Glsl::Vec4 flashColour_{1.0f, 1.0f, 1.0f, 1.0f};
    float flashDuration_ = 0.0f;
    float flashTimeCount_ = 0.0f;
    bool flashActive_ = false;

    std::shared_ptr<sf::Shader> toneShader_;
    sf::Glsl::Vec4 toneCurrentColour_{};
    sf::Glsl::Vec4 toneStartColour_{};
    sf::Glsl::Vec4 toneTargetColour_{};
    float toneDuration_ = 0.0f;
    float toneTimeCount_ = 0.0f;
    bool toneActive_ = false;
    std::unique_ptr<sf::RenderTexture> toneBuffer_;
    std::optional<sf::Sprite> toneBufferSprite_;

    float shakePower_ = 0.0f;
    float shakeSpeed_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeTimeCount_ = 0.0f;
    bool shakeActive_ = false;
    sf::Vector2f shakeOffset_{};
    float shakeNextUpdate_ = 0.0f;
    std::mt19937 random_{std::random_device{}()};
};

struct SceneStackRuntime {
    std::vector<std::shared_ptr<SceneRuntime>> scenes_;
    std::deque<std::shared_ptr<SceneRuntime>> retiredScenes_;
    std::deque<System::PendingSceneOperation> pendingSceneOperations_;
    std::mutex sceneMutex_;
    std::mutex pendingSceneMutex_;
    std::thread::id sceneOperationThread_;
};

struct LifecycleRuntime {
    std::function<void()> standardUpdate_;
    std::atomic_bool shuttingDown_ = false;
    std::mutex lifecycleMutex_;
    bool debugMode_ = false;
};

struct SystemRuntime {
    DisplayRuntime display;
    FramePipelineRuntime framePipeline;
    SceneStackRuntime sceneStack;
    LifecycleRuntime lifecycle;
};

SystemRuntime& runtime();

}  // namespace ludork::global::system_runtime
