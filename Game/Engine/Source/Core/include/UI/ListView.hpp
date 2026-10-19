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
    bool getChildScrollingEnabled() const;

    BIND_METHOD()
    void setChildScrollingEnabled(bool enabled);

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

    sf::FloatRect clipChildBounds(const ControlBase& child,
                                  const sf::FloatRect& absoluteBounds) const;

protected:
    static constexpr float ChildScrollSpeed = 30.0f;
    static constexpr float ChildScrollPauseDuration = 1.0f;

    sf::FloatRect _clipChildInteractionBounds(
        const ControlBase& child, const sf::FloatRect& bounds) const override;

    BIND_METHOD()
    virtual void draw(sf::RenderTarget& target,
                      sf::RenderStates states) const override;

private:
    struct ChildScrollState {
        sf::FloatRect bounds;
        sf::Vector2f origin;
        sf::Vector2f scale;
        float columnWidth = 0.0f;
        float distance = 0.0f;
        double elapsed = 0.0;
    };

    void setSizeValue(const sf::Vector2f& size);
    void updateChildScrolling(float deltaTime);
    static float childScrollOffset(const ChildScrollState& state);

    sf::Vector2f size_;
    int defaultItemHeight_ = 32;
    bool fixItemHeight_ = false;
    int columns_ = 1;
    std::vector<std::shared_ptr<ControlBase>> children_;
    std::vector<sf::FloatRect> itemLayoutRects_;
    std::vector<ChildScrollState> childScrollStates_;
    bool childScrollingEnabled_ = false;
    sf::RenderStates renderStates_;
    bool positionsSettled_ = false;
    float displayScale_ = 1.0f;
};
