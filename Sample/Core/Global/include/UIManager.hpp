#pragma once

#include <BindAnnotations.hpp>
#include <FocusManager.hpp>
#include <UI/ControlBase.hpp>

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

BIND_CLASS()
class UIManager {
public:
    BIND_INIT()
    UIManager();
    ~UIManager();

    BIND_METHOD(Pure = true)
    std::shared_ptr<FocusManager> getFocusManager() const;

    BIND_METHOD()
    void setFocusNavigationEnabled(bool enabled);

    BIND_METHOD()
    void registerFocusGroup(const std::shared_ptr<FocusGroup>& group);

    BIND_METHOD(outpins(default = nil))
    void loadUI(const std::shared_ptr<ControlBase>& ui);

    BIND_METHOD(Pure = true, returns = "uis")
    std::vector<std::shared_ptr<ControlBase>> getUIs() const;

    BIND_METHOD(outpins(default = nil))
    void removeUI(const std::shared_ptr<ControlBase>& ui);

    void fixedLogicHandle(float fixedDelta);
    void refreshDisplayScale();
    void logicHandle(float deltaTime);
    void renderHandle(float deltaTime,
                      const std::function<void()>& overlayRenderer = {});

    static void shutdown() noexcept;

private:
    std::vector<std::shared_ptr<ControlBase>> sortedUIs(bool descending) const;
    void activateFocusResolvers();
    void releaseRuntimeState() noexcept;
    static void deactivateFocusResolvers() noexcept;
    static std::shared_ptr<FunctionalBase> functionalUI(
        const std::shared_ptr<ControlBase>& ui);

    std::vector<std::shared_ptr<ControlBase>> uis_;
    std::shared_ptr<FocusManager> focusManager_;
    std::shared_ptr<RuntimeCallbackRegistry> callbackRegistry_;
    mutable std::mutex mutex_;
    float displayScale_ = 1.0f;
    bool released_ = false;

    static UIManager* activeManager_;
};
