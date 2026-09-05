#pragma once

#include <CoreMinimal.hpp>

#include <GlobalAnimation.hpp>
#include <CustomParticles/CommonTipController.hpp>
#include <Manager/TimeManager.hpp>
#include <Particles/ParticleSystem.hpp>
#include <System/SceneRuntime.hpp>
#include <UIManager.hpp>

#include <exception>
#include <mutex>
#include <thread>

namespace ludork::global::scene_base_impl {
class LifecycleRuntime;
}

BIND_CLASS(bind_bases = false, cast_bases = {"SceneRuntime"}, callbacks = true)
class SceneBase : public SceneRuntime {
public:
    BIND_INIT()
    SceneBase();
    ~SceneBase() override;

    BIND_METHOD(Pure = true)
    std::shared_ptr<UIManager> getUIManager() const;

    BIND_METHOD(latent(TimeUp = true), defaults = {nil, nil, {}, false},
                parameter_types = {float, function, any[], bool})
    TimerHandle addTimer(float interval, RuntimeIdentityPtr task = {},
                         RuntimeValue::Array params = {},
                         bool blocking = false);

    BIND_METHOD(metadata = false)
    TimerHandle addTimer(float interval, RuntimeIdentityPtr task,
                         bool blocking);

    BIND_METHOD(Pure = true, returns = "blocked")
    bool isInputBlocked() const;

    BIND_METHOD(outpins(default = nil))
    void addAnim(const std::shared_ptr<Animation>& anim);

    BIND_METHOD(Pure = true, returns = "anims")
    std::vector<std::shared_ptr<Animation>> getAnims() const;

    BIND_METHOD(outpins(default = nil))
    void removeAnim(const std::shared_ptr<Animation>& anim);

    BIND_METHOD(outpins(default = nil))
    void clearAnims();

    BIND_METHOD(outpins(default = nil))
    void addCommonTip(const std::string& text);

protected:
    BIND_METHOD()
    virtual void onEnter();

    BIND_METHOD()
    virtual void onQuit();

    BIND_METHOD()
    virtual void onCreate();

    BIND_METHOD()
    virtual void onInput();

    BIND_METHOD()
    virtual void onTick(float deltaTime);

    BIND_METHOD()
    virtual void onLateTick(float deltaTime);

    BIND_METHOD()
    virtual void onFixedTick(float fixedDelta);

    BIND_METHOD()
    virtual void onDestroy();

    BIND_METHOD(metadata = false)
    virtual void _drawSceneAnims();

    BIND_METHOD(metadata = false)
    virtual void _drawCommonTipOverlay();

    BIND_METHOD(metadata = false)
    virtual void _renderHandle(float deltaTime);

private:
    struct LogicStepPerformance {
        double sceneTickMilliseconds{};
        double maintenanceMilliseconds{};
        double fixedTickMilliseconds{};
        int fixedSteps{};
    };

    void systemMain() final;
    void systemEnter() final;
    void systemQuit() final;
    void systemDestroy() final;
    void systemShutdown() noexcept final;
    bool systemIsRunning() const noexcept final;
    void systemInput() final;

    void logicHandle(float deltaTime);
    void updateCommonTipOverlay(float deltaTime);
    void fixedLogicHandle(float fixedDelta);
    void releaseBlockingTimer(const std::shared_ptr<TimerEntry>& entry);
    LogicStepPerformance runLogicStep(float deltaTime, bool profile);
    void startLogicThread();
    void stopLogicThread() noexcept;
    void logicLoop();
    std::unique_lock<std::recursive_mutex> lockLogicDataForMain();
    std::exception_ptr takeLogicFailure();
    void clearRuntimeState() noexcept;

    float fixedAccumulator_ = 0.0f;
    float fixedStep_ = 1.0f / 60.0f;
    int maxFixedSteps_ = 5;
    mutable std::recursive_mutex logicDataMutex_;
    std::vector<std::shared_ptr<TimerEntry>> timerEntries_;
    std::vector<std::shared_ptr<Animation>> animations_;
    std::shared_ptr<UIManager> uiManager_;
    std::shared_ptr<ParticleSystem> commonTipParticleSystem_;
    std::shared_ptr<CommonTipController> commonTipController_;
    int blockingTimerCount_ = 0;
    std::thread logicThread_;
    std::mutex logicFailureMutex_;
    std::exception_ptr logicFailure_;
    std::unique_ptr<ludork::global::scene_base_impl::LifecycleRuntime>
        lifecycle_;
};
