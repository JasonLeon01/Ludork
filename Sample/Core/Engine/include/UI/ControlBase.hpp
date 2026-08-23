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
#include <string>
#include <vector>

class ControlBase;
class Canvas;
class FunctionalBase;

class ControlBaseSharedOwner
    : public std::enable_shared_from_this<ControlBase> {
protected:
    ControlBaseSharedOwner() = default;
    ControlBaseSharedOwner(const ControlBaseSharedOwner&) = default;
    ControlBaseSharedOwner& operator=(const ControlBaseSharedOwner&) = default;
    ~ControlBaseSharedOwner() = default;
};

BIND_CLASS(callbacks =
               "getChildren,getSize,getLocalBounds,getRenderStates,getOrigin,"
               "setOrigin,draw",
           cast_bases = "sf::Drawable,sf::Transformable")
class LUDORK_ENGINE_API ControlBase : public sf::Drawable,
                                      public sf::Transformable,
                                      public ControlBaseSharedOwner {
public:
    BIND_INIT()
    ControlBase() = default;
    virtual ~ControlBase() = default;

    BIND_METHOD(Pure = true)
    bool getVisible() const;

    BIND_METHOD()
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

    BIND_IGNORE()
    sf::Transform renderTransform() const;

    BIND_IGNORE()
    sf::Transform screenRenderTransform() const;

    BIND_IGNORE()
    virtual void refreshDisplayScale();

protected:
    BIND_METHOD(metadata = false)
    virtual sf::Transform _getScreenTransform() const;

    BIND_METHOD(metadata = false)
    virtual void _applyRenderStates(sf::RenderStates& states) const;

    BIND_METHOD(metadata = false)
    virtual sf::Transform _getRenderTransform() const;

    BIND_METHOD(metadata = false)
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
};
