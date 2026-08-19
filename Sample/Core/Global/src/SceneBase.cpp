#include <SceneBase.hpp>

#include "System/Diagnostics/PerformanceProfiler.hpp"

#include <Input/InputService.hpp>
#include <Manager/AudioManager.hpp>
#include <RuntimeSession.hpp>
#include <Utils/EventBus.hpp>
#include <VideoPlayback.hpp>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace {

double durationMilliseconds(std::chrono::steady_clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

}  // namespace

SceneBase::SceneBase()
    : uiManager_(std::make_shared<UIManager>()),
      commonTipParticleSystem_(std::make_shared<ParticleSystem>()),
      commonTipController_(
          std::make_shared<CommonTipController>(commonTipParticleSystem_)) {}

SceneBase::~SceneBase() {
    systemShutdown();
}

std::shared_ptr<UIManager> SceneBase::getUIManager() const {
    return uiManager_;
}

TimerHandle SceneBase::addTimer(float interval, RuntimeIdentityPtr task,
                                RuntimeValue::Array params, bool blocking) {
    const std::shared_ptr<TimerEntry> entry = std::make_shared<TimerEntry>(
        interval, std::move(task), std::move(params), blocking);
    {
        const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
        timerEntries_.push_back(entry);
        if (entry->blocking) {
            ++blockingTimerCount_;
        }
    }
    const std::weak_ptr<TimerEntry> weakEntry = entry;
    return [weakEntry]() {
        const std::shared_ptr<TimerEntry> activeEntry = weakEntry.lock();
        return activeEntry == nullptr || activeEntry->isReady();
    };
}

TimerHandle SceneBase::addTimer(float interval, RuntimeIdentityPtr task,
                                bool blocking) {
    return addTimer(interval, std::move(task), {}, blocking);
}

bool SceneBase::isInputBlocked() const {
    const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
    return blockingTimerCount_ > 0;
}

void SceneBase::addAnim(const std::shared_ptr<Animation>& anim) {
    if (anim == nullptr) {
        throw std::invalid_argument("Animation cannot be null");
    }
    const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
    animations_.push_back(anim);
}

std::vector<std::shared_ptr<Animation>> SceneBase::getAnims() const {
    const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
    return animations_;
}

void SceneBase::removeAnim(const std::shared_ptr<Animation>& anim) {
    const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
    const auto iterator =
        std::find(animations_.begin(), animations_.end(), anim);
    if (iterator == animations_.end()) {
        throw std::invalid_argument("Animation not found");
    }
    animations_.erase(iterator);
}

void SceneBase::clearAnims() {
    std::vector<std::shared_ptr<Animation>> animations;
    {
        const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
        animations.swap(animations_);
    }
}

void SceneBase::addCommonTip(const std::string& text) {
    std::shared_ptr<CommonTipController> controller;
    {
        const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
        controller = commonTipController_;
    }
    if (controller != nullptr) {
        controller->addTip(text);
    }
}

void SceneBase::systemMain() {
    ludork::standard::LuaExecutionPause luaExecutionPause;
    if (destroyed_ || mainRunning_.exchange(true)) {
        return;
    }
    std::exception_ptr failure;
    try {
        systemEnter();
        if (!destroyed_) {
            TimeManager::update();
            startLogicThread();
        }
        while (!runtimeStopping_.load() && System::isActive() &&
               System::getScene().get() == this) {
            PerformanceProfiler::beginMainFrame();
            const bool profile = PerformanceProfiler::isEnabled();
            MainFramePerformanceMeasurement measurement;
            std::chrono::steady_clock::time_point phaseStart;
            if (profile) {
                measurement.start = std::chrono::steady_clock::now();
                phaseStart = measurement.start;
            }
            {
                std::unique_lock<std::recursive_mutex> lock =
                    lockLogicDataForMain();
                if (System::hasPendingSceneOperations()) {
                    runtimeStopping_.store(true);
                    break;
                }
                System::updateRuntime();
                if (System::hasPendingSceneOperations()) {
                    runtimeStopping_.store(true);
                    break;
                }
            }
            if (profile) {
                const auto phaseEnd = std::chrono::steady_clock::now();
                measurement.runtimeMilliseconds =
                    durationMilliseconds(phaseEnd - phaseStart);
                phaseStart = phaseEnd;
            }
            float deltaTime = 0.0f;
            {
                std::unique_lock<std::recursive_mutex> lock =
                    lockLogicDataForMain();
                if (const std::shared_ptr<sf::RenderWindow> window =
                        System::getWindow();
                    window != nullptr) {
                    inputService().update(*window);
                }
                if (uiManager_ != nullptr) {
                    uiManager_->refreshDisplayScale();
                }
                systemInput();
                if (profile) {
                    const auto phaseEnd = std::chrono::steady_clock::now();
                    measurement.inputMilliseconds =
                        durationMilliseconds(phaseEnd - phaseStart);
                    phaseStart = phaseEnd;
                }
                TimeManager::update();
                deltaTime = TimeManager::getDeltaTime().asSeconds();
                if (uiManager_ != nullptr) {
                    uiManager_->logicHandle(deltaTime);
                }
                updateCommonTipOverlay(deltaTime);
                if (profile) {
                    const auto phaseEnd = std::chrono::steady_clock::now();
                    measurement.uiUpdateMilliseconds =
                        durationMilliseconds(phaseEnd - phaseStart);
                    phaseStart = phaseEnd;
                }
                _renderHandle(deltaTime);
                lock.unlock();
                System::present();
                if (profile) {
                    const auto phaseEnd = std::chrono::steady_clock::now();
                    measurement.renderMilliseconds =
                        durationMilliseconds(phaseEnd - phaseStart);
                }
                lock = lockLogicDataForMain();
                if (profile) {
                    phaseStart = std::chrono::steady_clock::now();
                }
                System::clearCanvas();
                if (profile) {
                    const auto phaseEnd = std::chrono::steady_clock::now();
                    measurement.renderMilliseconds +=
                        durationMilliseconds(phaseEnd - phaseStart);
                }
                if (profile) {
                    phaseStart = std::chrono::steady_clock::now();
                }
                onLateTick(deltaTime);
                if (profile) {
                    const auto phaseEnd = std::chrono::steady_clock::now();
                    measurement.lateUpdateMilliseconds =
                        durationMilliseconds(phaseEnd - phaseStart);
                }
                if (commonTipParticleSystem_ != nullptr) {
                    if (profile) {
                        phaseStart = std::chrono::steady_clock::now();
                    }
                    commonTipParticleSystem_->onLateTick(deltaTime);
                    if (profile) {
                        const auto phaseEnd = std::chrono::steady_clock::now();
                        measurement.uiUpdateMilliseconds +=
                            durationMilliseconds(phaseEnd - phaseStart);
                    }
                }
                if (profile) {
                    phaseStart = std::chrono::steady_clock::now();
                }
                System::completeFrame();
                if (profile) {
                    const auto phaseEnd = std::chrono::steady_clock::now();
                    measurement.renderMilliseconds +=
                        durationMilliseconds(phaseEnd - phaseStart);
                }
            }
            if (profile) {
                phaseStart = std::chrono::steady_clock::now();
            }
            AudioManager::update();
            if (profile) {
                measurement.end = std::chrono::steady_clock::now();
                measurement.audioMilliseconds =
                    durationMilliseconds(measurement.end - phaseStart);
                measurement.targetFps = System::getFrameRate();
                PerformanceProfiler::recordMainFrame(measurement);
            }
            failure = takeLogicFailure();
            if (failure != nullptr) {
                break;
            }
            std::this_thread::yield();
        }
    } catch (...) {
        failure = std::current_exception();
    }
    stopLogicThread();
    if (failure == nullptr) {
        failure = takeLogicFailure();
    }
    try {
        systemQuit();
    } catch (...) {
        if (failure == nullptr) {
            failure = std::current_exception();
        }
    }
    mainRunning_.store(false);

    try {
        System::drainRetiredScenes();
    } catch (...) {
        if (failure == nullptr) {
            failure = std::current_exception();
        }
    }
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
}

void SceneBase::systemEnter() {
    if (destroyed_) {
        return;
    }
    if (!created_) {
        onCreate();
        created_ = true;
    }
    if (entered_) {
        return;
    }
    entered_ = true;
    runtimeStopping_.store(false);
    fixedAccumulator_ = 0.0f;
    onEnter();
}

void SceneBase::systemQuit() {
    stopLogicThread();
    if (!entered_) {
        return;
    }
    entered_ = false;
    onQuit();
}

void SceneBase::systemDestroy() {
    stopLogicThread();
    if (destroyed_) {
        return;
    }
    std::exception_ptr failure;
    try {
        systemQuit();
    } catch (...) {
        failure = std::current_exception();
    }
    destroyed_ = true;
    if (created_) {
        try {
            onDestroy();
        } catch (...) {
            if (failure == nullptr) {
                failure = std::current_exception();
            }
        }
    }
    clearRuntimeState();
    if (failure != nullptr) {
        std::rethrow_exception(failure);
    }
}

void SceneBase::systemShutdown() noexcept {
    runtimeStopping_.store(true);
    stopLogicThread();
    entered_ = false;
    destroyed_ = true;
    clearRuntimeState();
}

bool SceneBase::systemIsRunning() const noexcept {
    return mainRunning_.load();
}

void SceneBase::systemInput() {
    if (entered_ && !destroyed_) {
        onInput();
    }
}

void SceneBase::onEnter() {
    System::setTransition();
}

void SceneBase::onQuit() {}

void SceneBase::onCreate() {}

void SceneBase::onInput() {}

void SceneBase::onTick(float deltaTime) {
    static_cast<void>(deltaTime);
}

void SceneBase::onLateTick(float deltaTime) {
    static_cast<void>(deltaTime);
}

void SceneBase::onFixedTick(float fixedDelta) {
    static_cast<void>(fixedDelta);
}

void SceneBase::onDestroy() {}

void SceneBase::_drawSceneAnims() {
    const std::vector<std::shared_ptr<Animation>> snapshot = getAnims();
    for (const std::shared_ptr<Animation>& animation : snapshot) {
        if (animation != nullptr) {
            System::draw(*animation);
        }
    }
}

void SceneBase::_drawCommonTipOverlay() {
    System::setWindowDefaultView();
    if (commonTipParticleSystem_ != nullptr) {
        System::draw(*commonTipParticleSystem_);
    }
}

void SceneBase::_renderHandle(float deltaTime) {
    _drawSceneAnims();
    if (uiManager_ != nullptr) {
        uiManager_->renderHandle(deltaTime, [this]() {
            _drawCommonTipOverlay();
        });
    }
}

void SceneBase::logicHandle(float deltaTime) {
    flushEvents();
    std::vector<std::shared_ptr<TimerEntry>> readyTimers;
    std::vector<std::shared_ptr<Animation>> animationSnapshot;
    {
        const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
        for (const std::shared_ptr<TimerEntry>& entry : timerEntries_) {
            entry->time = std::max(0.0f, entry->time - deltaTime);
            if (entry->isReady()) {
                readyTimers.push_back(entry);
            }
        }
        for (const std::shared_ptr<TimerEntry>& entry : readyTimers) {
            const auto iterator =
                std::find(timerEntries_.begin(), timerEntries_.end(), entry);
            if (iterator != timerEntries_.end()) {
                timerEntries_.erase(iterator);
            }
            releaseBlockingTimer(entry);
        }
        animationSnapshot = animations_;
    }
    for (const std::shared_ptr<TimerEntry>& entry : readyTimers) {
        if (!entry->isCancelled() && entry->task != nullptr) {
            resolveRuntime("reflect.invoke", {RuntimeValue(entry->task),
                                              RuntimeValue(entry->params)});
        }
    }
    std::vector<std::shared_ptr<Animation>> finishedAnimations;
    std::vector<std::shared_ptr<Animation>> activeAnimations;
    for (const std::shared_ptr<Animation>& animation : animationSnapshot) {
        if (animation == nullptr || animation->isFinished()) {
            finishedAnimations.push_back(animation);
        } else {
            activeAnimations.push_back(animation);
        }
    }
    {
        const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
        std::erase_if(
            animations_, [&finishedAnimations](const auto& animation) {
                return std::find(finishedAnimations.begin(),
                                 finishedAnimations.end(),
                                 animation) != finishedAnimations.end();
            });
    }
    for (const std::shared_ptr<Animation>& animation : activeAnimations) {
        animation->update(deltaTime);
    }
}

void SceneBase::updateCommonTipOverlay(float deltaTime) {
    std::shared_ptr<CommonTipController> controller;
    std::shared_ptr<ParticleSystem> particleSystem;
    {
        const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
        controller = commonTipController_;
        particleSystem = commonTipParticleSystem_;
    }
    if (controller != nullptr) {
        controller->onTick(deltaTime);
    }
    if (particleSystem != nullptr) {
        particleSystem->onTick(deltaTime);
    }
}

void SceneBase::fixedLogicHandle(float fixedDelta) {
    if (uiManager_ != nullptr) {
        uiManager_->fixedLogicHandle(fixedDelta);
    }
}

void SceneBase::releaseBlockingTimer(const std::shared_ptr<TimerEntry>& entry) {
    if (entry == nullptr || !entry->blocking) {
        return;
    }
    entry->blocking = false;
    blockingTimerCount_ = std::max(0, blockingTimerCount_ - 1);
}

SceneBase::LogicStepPerformance SceneBase::runLogicStep(float deltaTime,
                                                        bool profile) {
    const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
    LogicStepPerformance performance;
    if (runtimeStopping_.load() || !System::isActive() ||
        System::getScene().get() != this ||
        System::hasPendingSceneOperations()) {
        return performance;
    }
    std::chrono::steady_clock::time_point phaseStart;
    if (profile) {
        phaseStart = std::chrono::steady_clock::now();
    }
    onTick(deltaTime);
    if (profile) {
        const auto phaseEnd = std::chrono::steady_clock::now();
        performance.sceneTickMilliseconds =
            durationMilliseconds(phaseEnd - phaseStart);
    }
    if (profile) {
        phaseStart = std::chrono::steady_clock::now();
    }
    logicHandle(deltaTime);
    if (profile) {
        const auto phaseEnd = std::chrono::steady_clock::now();
        performance.maintenanceMilliseconds =
            durationMilliseconds(phaseEnd - phaseStart);
    }
    const int targetFps = std::max(1, System::getFrameRate());
    fixedStep_ = 1.0f / static_cast<float>(targetFps);
    fixedAccumulator_ += deltaTime;
    int steps = 0;
    while (fixedAccumulator_ >= fixedStep_ && steps < maxFixedSteps_) {
        if (profile) {
            phaseStart = std::chrono::steady_clock::now();
        }
        onFixedTick(fixedStep_);
        if (profile) {
            const auto phaseEnd = std::chrono::steady_clock::now();
            performance.fixedTickMilliseconds +=
                durationMilliseconds(phaseEnd - phaseStart);
            phaseStart = std::chrono::steady_clock::now();
        }
        fixedLogicHandle(fixedStep_);
        if (profile) {
            const auto phaseEnd = std::chrono::steady_clock::now();
            performance.maintenanceMilliseconds +=
                durationMilliseconds(phaseEnd - phaseStart);
        }
        fixedAccumulator_ -= fixedStep_;
        ++steps;
    }
    performance.fixedSteps = steps;
    return performance;
}

void SceneBase::startLogicThread() {
    if (logicThread_.joinable()) {
        return;
    }
    runtimeStopping_.store(false);
    {
        const std::lock_guard<std::mutex> lock(logicFailureMutex_);
        logicFailure_ = nullptr;
    }
    logicThread_ = std::thread([this]() {
        try {
            logicLoop();
        } catch (...) {
            const std::lock_guard<std::mutex> lock(logicFailureMutex_);
            logicFailure_ = std::current_exception();
            runtimeStopping_.store(true);
        }
    });
}

void SceneBase::stopLogicThread() noexcept {
    runtimeStopping_.store(true);
    if (!logicThread_.joinable()) {
        return;
    }
    if (logicThread_.get_id() != std::this_thread::get_id()) {
        logicThread_.join();
    }
}

void SceneBase::logicLoop() {
    fixedAccumulator_ = 0.0f;
    auto lastTime = std::chrono::steady_clock::now();
    std::uint64_t videoPlaybackSequence = getVideoPlaybackCompletionSequence();
    while (!runtimeStopping_.load() && System::isActive() &&
           System::getScene().get() == this &&
           !System::hasPendingSceneOperations()) {
        const int targetFps = std::max(1, System::getFrameRate());
        const auto logicFrameTime =
            std::chrono::duration<double>(1.0 / static_cast<double>(targetFps));
        const auto frameStart = std::chrono::steady_clock::now();
        const float deltaTime =
            std::max(
                0.0f,
                std::chrono::duration<float>(frameStart - lastTime).count()) *
            TimeManager::getSpeed();
        lastTime = frameStart;
        const bool profile = PerformanceProfiler::isEnabled();
        const LogicStepPerformance stepPerformance =
            runLogicStep(deltaTime, profile);
        const auto workEnd = std::chrono::steady_clock::now();
        const std::uint64_t currentVideoPlaybackSequence =
            getVideoPlaybackCompletionSequence();
        if (currentVideoPlaybackSequence != videoPlaybackSequence) {
            lastTime = workEnd;
            fixedAccumulator_ = 0.0f;
            videoPlaybackSequence = currentVideoPlaybackSequence;
        }
        const auto elapsed = workEnd - frameStart;
        if (elapsed < logicFrameTime) {
            std::this_thread::sleep_for(logicFrameTime - elapsed);
        }
        if (profile) {
            const auto frameEnd = std::chrono::steady_clock::now();
            LogicTickPerformanceMeasurement measurement;
            measurement.start = frameStart;
            measurement.end = frameEnd;
            measurement.sceneTickMilliseconds =
                stepPerformance.sceneTickMilliseconds;
            measurement.maintenanceMilliseconds =
                stepPerformance.maintenanceMilliseconds;
            measurement.fixedTickMilliseconds =
                stepPerformance.fixedTickMilliseconds;
            measurement.sleepMilliseconds =
                durationMilliseconds(frameEnd - workEnd);
            measurement.fixedSteps = stepPerformance.fixedSteps;
            PerformanceProfiler::recordLogicTick(measurement);
        }
    }
}

std::unique_lock<std::recursive_mutex> SceneBase::lockLogicDataForMain() {
    std::unique_lock<std::recursive_mutex> lock(logicDataMutex_,
                                                std::defer_lock);
    while (true) {
        processPendingVideoPlayback();
        if (lock.try_lock()) {
            return lock;
        }
        std::this_thread::yield();
    }
}

std::exception_ptr SceneBase::takeLogicFailure() {
    const std::lock_guard<std::mutex> lock(logicFailureMutex_);
    std::exception_ptr failure = logicFailure_;
    logicFailure_ = nullptr;
    return failure;
}

void SceneBase::clearRuntimeState() noexcept {
    std::vector<std::shared_ptr<TimerEntry>> timerEntries;
    std::vector<std::shared_ptr<Animation>> animations;
    std::shared_ptr<CommonTipController> commonTipController;
    std::shared_ptr<ParticleSystem> commonTipParticleSystem;
    std::shared_ptr<UIManager> uiManager;
    {
        const std::lock_guard<std::recursive_mutex> lock(logicDataMutex_);
        timerEntries.swap(timerEntries_);
        animations.swap(animations_);
        commonTipController = std::move(commonTipController_);
        commonTipParticleSystem = std::move(commonTipParticleSystem_);
        uiManager = std::move(uiManager_);
        blockingTimerCount_ = 0;
    }
}
