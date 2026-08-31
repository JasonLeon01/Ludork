#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>

#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/Graphics/Transformable.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

class ControlBase;
class Canvas;
class FunctionalBase;

class RuntimeCallbackReleasable {
public:
    virtual ~RuntimeCallbackReleasable() = 0;
    virtual void releaseRuntimeCallbacks() noexcept = 0;
};

inline RuntimeCallbackReleasable::~RuntimeCallbackReleasable() = default;

class LUDORK_ENGINE_API RuntimeCallbackRegistry {
public:
    void registerControl(ControlBase* control);
    void unregisterControl(ControlBase* control) noexcept;
    void releaseRuntimeCallbacks() noexcept;

private:
    bool contains(ControlBase* control) const noexcept;

    mutable std::mutex mutex_;
    std::unordered_set<ControlBase*> controls_;
};

class ControlBaseSharedOwner
    : public std::enable_shared_from_this<ControlBase> {
protected:
    ControlBaseSharedOwner() = default;
    ControlBaseSharedOwner(const ControlBaseSharedOwner&) = default;
    ControlBaseSharedOwner& operator=(const ControlBaseSharedOwner&) = default;
    ~ControlBaseSharedOwner() = default;
};

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API ControlBase : public sf::Drawable,
                                      public sf::Transformable,
                                      public ControlBaseSharedOwner,
                                      public RuntimeCallbackReleasable {
public:
    BIND_INIT()
    ControlBase();
    virtual ~ControlBase();

    BIND_METHOD(Pure = true)
    bool getVisible() const;

    BIND_METHOD(callback = false)
    virtual void setVisible(bool visible);

    BIND_METHOD(Pure = true)
    const std::string& getName() const;

    BIND_METHOD()
    void setName(const std::string& name);

    BIND_METHOD(Pure = true)
    std::shared_ptr<ControlBase> getParent() const;

    BIND_METHOD()
    void setParent(const std::shared_ptr<ControlBase>& parent);

    BIND_METHOD(Pure = true)
    virtual std::vector<std::shared_ptr<ControlBase>> getChildren() const;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const;

    BIND_METHOD(Pure = true)
    sf::FloatRect getAbsoluteBounds() const;

    BIND_METHOD(Pure = true)
    virtual sf::RenderStates getRenderStates() const;

    BIND_METHOD()
    void setPosition(const sf::Vector2f& position);

    BIND_METHOD(Pure = true)
    sf::Vector2f getPosition() const;

    BIND_METHOD()
    void move(const sf::Vector2f& offset);

    BIND_METHOD(Pure = true)
    sf::Angle getRotation() const;

    BIND_METHOD()
    void setRotation(sf::Angle angle);

    BIND_METHOD(name = "setRotation", metadata = false)
    void setRotationDegrees(float angle);

    BIND_METHOD()
    void rotate(sf::Angle angle);

    BIND_METHOD(name = "rotate", metadata = false)
    void rotateDegrees(float angle);

    BIND_METHOD(Pure = true)
    sf::Vector2f getScale() const;

    BIND_METHOD()
    void setScale(const sf::Vector2f& scale);

    BIND_METHOD()
    void scale(const sf::Vector2f& factor);

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getOrigin() const;

    BIND_METHOD()
    virtual void setOrigin(const sf::Vector2f& origin);

    BIND_METHOD(Pure = true)
    sf::Transform getTransform() const;

    BIND_METHOD(Pure = true)
    sf::Transform getInverseTransform() const;

    sf::Transform renderTransform() const;

    sf::Transform screenRenderTransform() const;

    virtual void refreshDisplayScale();

    void releaseRuntimeCallbacks() noexcept override;

    void adoptRuntimeCallbackRegistry(
        const std::shared_ptr<RuntimeCallbackRegistry>& registry);

    static void activateRuntimeCallbackRegistry(
        const std::shared_ptr<RuntimeCallbackRegistry>& registry) noexcept;

    static void deactivateRuntimeCallbackRegistry(
        const std::shared_ptr<RuntimeCallbackRegistry>& registry) noexcept;

    static void resetActiveRuntimeCallbackRegistry() noexcept;

protected:
    BIND_METHOD(callback = false, metadata = false)
    virtual sf::Transform _getScreenTransform() const;

    BIND_METHOD(callback = false, metadata = false)
    virtual void _applyRenderStates(sf::RenderStates& states) const;

    BIND_METHOD(callback = false, metadata = false)
    virtual sf::Transform _getRenderTransform() const;

    BIND_METHOD(callback = false, metadata = false)
    virtual sf::Transform _getScreenRenderTransform() const;

    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    friend class Canvas;
    friend class FunctionalBase;

    static void resetFunctionalInteractions(ControlBase& control);

    virtual bool _hasOverlay() const;
    virtual void _drawOverlay(sf::RenderTarget& target,
                              sf::RenderStates states) const;

    bool visible_ = true;
    std::string name_;
    std::weak_ptr<ControlBase> parent_;
    std::weak_ptr<RuntimeCallbackRegistry> runtimeCallbackRegistry_;

    static std::weak_ptr<RuntimeCallbackRegistry>
        activeRuntimeCallbackRegistry_;
};
