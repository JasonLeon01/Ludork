#pragma once

#include "SystemRuntime.hpp"

namespace ludork::global::system_runtime {

inline SystemRuntime& systemRuntimeState = runtime();
inline auto& window_ = systemRuntimeState.display.window_;
inline auto& windowMutex_ = systemRuntimeState.display.windowMutex_;
inline auto& cursor_ = systemRuntimeState.display.cursor_;
inline auto& windowTitle_ = systemRuntimeState.display.windowTitle_;
inline auto& windowIconPath_ = systemRuntimeState.display.windowIconPath_;
inline auto& windowCursorPath_ = systemRuntimeState.display.windowCursorPath_;
inline auto& windowContextSettings_ =
    systemRuntimeState.display.windowContextSettings_;
inline auto& observedWindowSize_ =
    systemRuntimeState.display.observedWindowSize_;
inline auto& observedWindowClientSize_ =
    systemRuntimeState.display.observedWindowClientSize_;
inline auto& pendingConfiguredScale_ =
    systemRuntimeState.display.pendingConfiguredScale_;
inline auto& pendingResizeScale_ =
    systemRuntimeState.display.pendingResizeScale_;
inline auto& lastResizeTime_ = systemRuntimeState.display.lastResizeTime_;
inline auto& desktopFullscreen_ = systemRuntimeState.display.desktopFullscreen_;
inline auto& inputMethodDisabled_ =
    systemRuntimeState.display.inputMethodDisabled_;
inline auto& canvasDefaultViewActive_ =
    systemRuntimeState.display.canvasDefaultViewActive_;
inline auto& canvas_ = systemRuntimeState.display.canvas_;
inline auto& canvasSprite_ = systemRuntimeState.display.canvasSprite_;
inline auto& transition_ = systemRuntimeState.framePipeline.transition_;
inline auto& transitionTempTexture_ =
    systemRuntimeState.framePipeline.transitionTempTexture_;
inline auto& transitionOutputTexture_ =
    systemRuntimeState.framePipeline.transitionOutputTexture_;
inline auto& transitionMaskTexture_ =
    systemRuntimeState.framePipeline.transitionMaskTexture_;
inline auto& transitionSprite_ =
    systemRuntimeState.framePipeline.transitionSprite_;
inline auto& transitionOutputSprite_ =
    systemRuntimeState.framePipeline.transitionOutputSprite_;
inline auto& graphicsCanvases_ =
    systemRuntimeState.framePipeline.graphicsCanvases_;
inline auto& graphicsShaders_ =
    systemRuntimeState.framePipeline.graphicsShaders_;
inline auto& transitionShader_ =
    systemRuntimeState.framePipeline.transitionShader_;
inline auto& transitionResource_ =
    systemRuntimeState.framePipeline.transitionResource_;
inline auto& inTransition_ = systemRuntimeState.framePipeline.inTransition_;
inline auto& transitionTimeCount_ =
    systemRuntimeState.framePipeline.transitionTimeCount_;
inline auto& transitionTime_ = systemRuntimeState.framePipeline.transitionTime_;
inline auto& transitionRevision_ =
    systemRuntimeState.framePipeline.transitionRevision_;
inline auto& composedTransitionRevision_ =
    systemRuntimeState.framePipeline.composedTransitionRevision_;
inline auto& transitionCompletionPending_ =
    systemRuntimeState.framePipeline.transitionCompletionPending_;
inline auto& transitionFrozen_ =
    systemRuntimeState.framePipeline.transitionFrozen_;
inline auto& transitionFreezePending_ =
    systemRuntimeState.framePipeline.transitionFreezePending_;
inline auto& pendingTransition_ =
    systemRuntimeState.framePipeline.pendingTransition_;
inline auto& transitionMutex_ =
    systemRuntimeState.framePipeline.transitionMutex_;
inline auto& presentMutex_ = systemRuntimeState.framePipeline.presentMutex_;
inline auto& flashShader_ = systemRuntimeState.framePipeline.flashShader_;
inline auto& flashColour_ = systemRuntimeState.framePipeline.flashColour_;
inline auto& flashDuration_ = systemRuntimeState.framePipeline.flashDuration_;
inline auto& flashTimeCount_ = systemRuntimeState.framePipeline.flashTimeCount_;
inline auto& flashActive_ = systemRuntimeState.framePipeline.flashActive_;
inline auto& toneShader_ = systemRuntimeState.framePipeline.toneShader_;
inline auto& toneCurrentColour_ =
    systemRuntimeState.framePipeline.toneCurrentColour_;
inline auto& toneStartColour_ =
    systemRuntimeState.framePipeline.toneStartColour_;
inline auto& toneTargetColour_ =
    systemRuntimeState.framePipeline.toneTargetColour_;
inline auto& toneDuration_ = systemRuntimeState.framePipeline.toneDuration_;
inline auto& toneTimeCount_ = systemRuntimeState.framePipeline.toneTimeCount_;
inline auto& toneActive_ = systemRuntimeState.framePipeline.toneActive_;
inline auto& toneBuffer_ = systemRuntimeState.framePipeline.toneBuffer_;
inline auto& toneBufferSprite_ =
    systemRuntimeState.framePipeline.toneBufferSprite_;
inline auto& shakePower_ = systemRuntimeState.framePipeline.shakePower_;
inline auto& shakeSpeed_ = systemRuntimeState.framePipeline.shakeSpeed_;
inline auto& shakeDuration_ = systemRuntimeState.framePipeline.shakeDuration_;
inline auto& shakeTimeCount_ = systemRuntimeState.framePipeline.shakeTimeCount_;
inline auto& shakeActive_ = systemRuntimeState.framePipeline.shakeActive_;
inline auto& shakeOffset_ = systemRuntimeState.framePipeline.shakeOffset_;
inline auto& shakeNextUpdate_ =
    systemRuntimeState.framePipeline.shakeNextUpdate_;
inline auto& random_ = systemRuntimeState.framePipeline.random_;
inline auto& scenes_ = systemRuntimeState.sceneStack.scenes_;
inline auto& retiredScenes_ = systemRuntimeState.sceneStack.retiredScenes_;
inline auto& pendingSceneOperations_ =
    systemRuntimeState.sceneStack.pendingSceneOperations_;
inline auto& sceneMutex_ = systemRuntimeState.sceneStack.sceneMutex_;
inline auto& pendingSceneMutex_ =
    systemRuntimeState.sceneStack.pendingSceneMutex_;
inline auto& sceneOperationThread_ =
    systemRuntimeState.sceneStack.sceneOperationThread_;
inline auto& standardUpdate_ = systemRuntimeState.lifecycle.standardUpdate_;
inline auto& shuttingDown_ = systemRuntimeState.lifecycle.shuttingDown_;
inline auto& lifecycleMutex_ = systemRuntimeState.lifecycle.lifecycleMutex_;
inline auto& debugMode_ = systemRuntimeState.lifecycle.debugMode_;

}  // namespace ludork::global::system_runtime
