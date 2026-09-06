#pragma once

#include <CoreMinimal.hpp>

class FocusGroup;
class FunctionalBase;

BIND_CLASS()
class FocusManager {
public:
    BIND_INIT()
    FocusManager() = default;
    ~FocusManager();

    void shutdown() noexcept;

    BIND_METHOD()
    void setNavigationEnabled(bool enabled);

    BIND_METHOD()
    bool getNavigationEnabled() const;

    BIND_METHOD()
    bool isRoutingKeyboard() const;

    BIND_METHOD()
    void registerElement(const std::shared_ptr<FunctionalBase>& element);

    BIND_METHOD()
    void unregisterElement(const std::shared_ptr<FunctionalBase>& element);

    BIND_METHOD()
    void registerFocusGroup(const std::shared_ptr<FocusGroup>& group);

    BIND_METHOD()
    void unregisterFocusGroup(const std::shared_ptr<FocusGroup>& group);

    BIND_METHOD()
    std::shared_ptr<FunctionalBase> getFocus() const;

    BIND_METHOD()
    bool setFocus(const std::shared_ptr<FunctionalBase>& element);

    BIND_METHOD()
    void clearFocus();

    BIND_METHOD()
    void prepareFrame();

    BIND_METHOD()
    bool shouldDispatchKeyboardTo(
        const std::shared_ptr<FunctionalBase>& element) const;

    BIND_METHOD()
    bool isFocused(const std::shared_ptr<FunctionalBase>& element) const;

    BIND_METHOD()
    bool isCursorFocusOwner(
        const std::shared_ptr<FunctionalBase>& element) const;

    BIND_METHOD()
    bool requestDirectionalMove(const std::shared_ptr<FunctionalBase>& element,
                                const std::string& direction);

    BIND_METHOD(defaults = {nil})
    bool moveFocus(const std::string& direction,
                   const std::shared_ptr<FunctionalBase>& source = nullptr);

    BIND_METHOD()
    bool activateGroup(const std::shared_ptr<FocusGroup>& group);

    bool setFocus(FunctionalBase& element);
    bool shouldDispatchKeyboardTo(const FunctionalBase& element) const;
    bool isCursorFocusOwner(const FunctionalBase& element) const;
    bool requestDirectionalMove(FunctionalBase& element,
                                const std::string& direction);

private:
    std::shared_ptr<FunctionalBase> findDefaultFocus() const;
    std::shared_ptr<FunctionalBase> findDirectionalTarget(
        const std::shared_ptr<FocusGroup>& group,
        const std::string& direction) const;
    bool isFocusable(const std::shared_ptr<FunctionalBase>& element) const;
    std::shared_ptr<FocusGroup> findGroupForElement(
        const std::shared_ptr<FunctionalBase>& element) const;
    std::vector<std::shared_ptr<FocusGroup>> allGroups() const;
    std::shared_ptr<FunctionalBase> findElement(
        const FunctionalBase* element) const;

    std::shared_ptr<FunctionalBase> focusedElement_;
    std::shared_ptr<FunctionalBase> cursorFocusElement_;
    std::vector<std::shared_ptr<FocusGroup>> focusGroups_;
    std::unordered_map<FunctionalBase*, std::shared_ptr<FocusGroup>>
        autoFocusGroups_;
    bool navigationEnabled_ = false;
};
