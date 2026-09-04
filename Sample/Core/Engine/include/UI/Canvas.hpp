#pragma once

#include <EngineRuntimeApi.hpp>

#include <Animation.hpp>
#include <LudorkRuntimeBinding/Annotations.hpp>
#include <UI/FunctionalBase.hpp>
#include <UI/SpriteBase.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <vector>

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API Canvas : public SpriteBase, public FunctionalBase {
public:
    BIND_INIT()
    explicit Canvas(const sf::IntRect& rect);
    virtual ~Canvas() = default;

    BIND_METHOD(Pure = true)
    sf::Vector2f getOrigin() const override;

    BIND_METHOD()
    void setOrigin(const sf::Vector2f& origin) override;

    BIND_METHOD(Pure = true, lua_return_type = "sf::Vector2u")
    virtual sf::Vector2f getSize() const override;

    BIND_METHOD()
    void resize(const sf::Vector2u& size);

    BIND_METHOD(Pure = true)
    sf::IntRect getNoTranslationRect() const;

    BIND_METHOD(Pure = true)
    virtual sf::IntRect getContentRect() const;

    BIND_METHOD(Pure = true)
    sf::View getView() const;

    BIND_METHOD(Pure = true)
    sf::View getDefaultView() const;

    BIND_METHOD()
    void setView(const sf::View& view);

    BIND_METHOD(Pure = true)
    virtual std::vector<std::shared_ptr<ControlBase>> getChildren()
        const override;

    BIND_METHOD()
    void addChild(const std::shared_ptr<ControlBase>& child);

    BIND_METHOD()
    void removeChild(const std::shared_ptr<ControlBase>& child);

    BIND_METHOD()
    void addAnim(const std::shared_ptr<AnimSprite>& animation);

    BIND_METHOD()
    void removeAnim(const std::shared_ptr<AnimSprite>& animation);

    BIND_METHOD()
    void clearAnims();

    BIND_METHOD(Pure = true)
    std::vector<std::shared_ptr<AnimSprite>> getAnims() const;

    BIND_METHOD()
    void setZOrder(int zOrder);

    BIND_METHOD(Pure = true)
    int getZOrder() const;

    BIND_METHOD()
    virtual void update(float deltaTime) override;

    BIND_METHOD()
    virtual void render();

    void render(sf::RenderTarget& target);

    BIND_METHOD()
    virtual void lateUpdate(float deltaTime) override;

    BIND_METHOD()
    virtual void fixedUpdate(float fixedDelta) override;

    sf::RenderTexture& getRenderTexture();

    const sf::RenderTexture& getRenderTexture() const;

    void refreshDisplayScale() override;

protected:
    BIND_METHOD(metadata = false)
    virtual sf::Transform _getScreenRenderTransform() const override;

    BIND_METHOD(callback = false, metadata = false)
    virtual void _appendRenderNode(const std::shared_ptr<ControlBase>& node,
                                   const sf::RenderStates& parentStates);

    BIND_METHOD(callback = false, metadata = false)
    virtual void _buildRenderQueue();

    BIND_METHOD(callback = false, metadata = false)
    virtual sf::RenderStates _getAnimRenderStates() const;

private:
    struct RenderEntry {
        std::shared_ptr<ControlBase> node;
        sf::RenderStates states;
    };

    static std::shared_ptr<sf::Texture> placeholderTexture();
    bool hasCanvasAncestor() const;
    void appendOverlayNode(const std::shared_ptr<ControlBase>& node);
    void buildOverlayQueue();
    void drawOverlays();
    void bindCanvasTexture();

    sf::IntRect inRect_;
    sf::Vector2u size_;
    std::shared_ptr<sf::RenderTexture> canvas_;
    std::vector<std::shared_ptr<ControlBase>> children_;
    std::vector<RenderEntry> renderQueue_;
    std::vector<std::shared_ptr<ControlBase>> overlayQueue_;
    std::vector<std::shared_ptr<AnimSprite>> animations_;
    int zOrder_ = 0;
    float displayScale_ = 1.0f;
};
