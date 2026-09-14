#pragma once

#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>

class FocusNeighbor;
class FocusManager;
class FunctionalBase;

BIND_CLASS(callbacks = true)
class FocusGroup : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(FocusGroup, RuntimeObject)

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
