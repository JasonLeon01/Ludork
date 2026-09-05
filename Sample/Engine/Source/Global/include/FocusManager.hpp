#pragma once

#include <CoreMinimal.hpp>

#include <UI/FunctionalBase.hpp>

class FocusGroup;

BIND_CLASS()
class FocusTransition {
public:
    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string DIRECTIONAL;

    BIND_CLASS_PROPERTY(readonly = true)
    static const std::string EXPLICIT;
};

BIND_CLASS()
class FocusNeighbor {
public:
    BIND_INIT(defaults = {directional})
    FocusNeighbor(std::shared_ptr<FocusGroup> group,
                  std::string transition = "directional");

    BIND_METHOD(property = "group", setter = "setGroup")
    std::shared_ptr<FocusGroup> getGroup() const;

    void setGroup(const std::shared_ptr<FocusGroup>& group);

    BIND_PROPERTY()
    std::string transition;

private:
    std::weak_ptr<FocusGroup> group_;
};

BIND_CLASS(callbacks = true)
class FocusGroup : public RuntimeObject {
public:
    BIND_INIT(defaults = {{}, nil})
    explicit FocusGroup(std::string name,
                        std::vector<std::shared_ptr<FunctionalBase>> items = {},
                        std::shared_ptr<FunctionalBase> activeOwner = nullptr);

    BIND_PROPERTY()
    std::string name;

    BIND_PROPERTY()
    std::shared_ptr<FunctionalBase> activeOwner;

    BIND_METHOD()
    void addItem(const std::shared_ptr<FunctionalBase>& item);

    BIND_METHOD()
    void removeItem(const std::shared_ptr<FunctionalBase>& item);

    BIND_METHOD()
    std::vector<std::shared_ptr<FunctionalBase>> getItems() const;

    BIND_METHOD(defaults = {directional})
    void setNeighbor(const std::string& direction,
                     const std::shared_ptr<FocusGroup>& neighbor,
                     const std::string& transition = "directional");

    BIND_METHOD(metadata = false)
    void setNeighbor(const std::string& direction,
                     const std::shared_ptr<FocusNeighbor>& neighbor);

    BIND_METHOD()
    std::shared_ptr<FocusNeighbor> getNeighbor(
        const std::string& direction) const;

    BIND_METHOD()
    bool canEnter() const;

    BIND_METHOD()
    std::shared_ptr<FunctionalBase> findInitialFocus() const;

    BIND_METHOD()
    void rememberFocus(const std::shared_ptr<FunctionalBase>& element);

    BIND_METHOD()
    virtual std::shared_ptr<FunctionalBase> moveWithin(
        const std::shared_ptr<FunctionalBase>& current,
        const std::string& direction);

private:
    void releaseRuntimeState() noexcept;
    bool contains(const std::shared_ptr<FunctionalBase>& element) const;
    std::shared_ptr<FunctionalBase> findInitialFocusLocal() const;
    static bool isLocallyFocusable(
        const std::shared_ptr<FunctionalBase>& element);
    static bool isOwnerAvailable(
        const std::shared_ptr<FunctionalBase>& element);

    std::vector<std::shared_ptr<FunctionalBase>> items_;
    std::unordered_map<std::string, std::shared_ptr<FocusNeighbor>>
        neighborMap_;
    std::weak_ptr<FunctionalBase> lastFocusedElement_;
    std::weak_ptr<FocusGroup> self_;

    friend class FocusManager;
};

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
