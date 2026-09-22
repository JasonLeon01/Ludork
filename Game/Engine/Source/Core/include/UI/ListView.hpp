#pragma once

#include <CoreMinimal.hpp>

#include <EngineRuntimeApi.hpp>
#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API ListView : public ControlBase, public FunctionalBase {
public:
    LUDORK_CAST_DERIVED(ListView, ControlBase, FunctionalBase)

    BIND_INIT()
    ListView(const sf::IntRect& rect, int defaultItemHeight = 32,
             bool fixItemHeight = false, int columns = 1);
    virtual ~ListView() = default;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getOrigin() const override;

    BIND_METHOD()
    virtual void setOrigin(const sf::Vector2f& origin) override;

    BIND_METHOD(Pure = true)
    int getColumns() const;

    /// Return the column width and authored default row height in logical
    /// pixels.
    BIND_METHOD(Pure = true)
    sf::Vector2f getDefaultItemSize() const;

    /// Return a zero-based item's cell in list-local logical coordinates.
    /// Variable-height rows use the tallest item in that row. An invalid index
    /// throws.
    BIND_METHOD(Pure = true)
    sf::FloatRect getItemLayoutRect(int index) const;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getContentBounds() const override;

    BIND_METHOD()
    void setSize(const sf::Vector2i& size);

    BIND_METHOD(name = "setSize", metadata = false)
    void setSizeVector2u(const sf::Vector2u& size);

    BIND_METHOD(name = "setSize", metadata = false)
    void setSizeVector2f(const sf::Vector2f& size);

    BIND_METHOD()
    void setColumns(int columns);

    BIND_METHOD(Pure = true)
    virtual std::vector<std::shared_ptr<ControlBase>> getChildren()
        const override;

    BIND_METHOD()
    void addChild(const std::shared_ptr<ControlBase>& child);

    BIND_METHOD()
    void removeChild(const std::shared_ptr<ControlBase>& child);

    BIND_METHOD()
    void clearChildren();

    BIND_METHOD(Pure = true)
    virtual sf::RenderStates getRenderStates() const override;

    BIND_METHOD()
    virtual void update(float deltaTime) override;

    BIND_METHOD()
    virtual void lateUpdate(float deltaTime) override;

    BIND_METHOD()
    virtual void fixedUpdate(float fixedDelta) override;

    BIND_METHOD()
    void invalidatePositions();

    BIND_METHOD()
    virtual void applyPositions();

    void refreshDisplayScale() override;

protected:
    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    void setSizeValue(const sf::Vector2f& size);

    sf::Vector2f size_;
    int defaultItemHeight_ = 32;
    bool fixItemHeight_ = false;
    int columns_ = 1;
    std::vector<std::shared_ptr<ControlBase>> children_;
    std::vector<sf::FloatRect> itemLayoutRects_;
    sf::RenderStates renderStates_;
    bool positionsSettled_ = false;
    float displayScale_ = 1.0f;
};
