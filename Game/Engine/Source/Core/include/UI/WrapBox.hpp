#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <UI/ControlBase.hpp>
#include <UI/FunctionalBase.hpp>

class UiAssetInstance;

BIND_CLASS(callbacks = true)
class LUDORK_ENGINE_API WrapBox : public ControlBase, public FunctionalBase {
public:
    LUDORK_CAST_DERIVED(WrapBox, ControlBase, FunctionalBase)

    BIND_INIT()
    explicit WrapBox(const sf::Vector2f& size, int count = 1,
                     const sf::Vector2f& spacing = sf::Vector2f{0.0f, 0.0f});
    ~WrapBox() override;

    BIND_METHOD(Pure = true)
    sf::Vector2f getSize() const override;
    BIND_METHOD()
    void setSize(const sf::Vector2f& size);
    BIND_METHOD(Pure = true)
    int getCount() const;
    BIND_METHOD()
    void setCount(int count);
    BIND_METHOD(Pure = true)
    sf::Vector2f getSpacing() const;
    BIND_METHOD()
    void setSpacing(const sf::Vector2f& spacing);
    BIND_METHOD(Pure = true)
    std::shared_ptr<ControlBase> get(int index) const;
    BIND_METHOD(Pure = true)
    std::vector<std::shared_ptr<ControlBase>> getChildren() const override;
    BIND_METHOD(Pure = true)
    sf::FloatRect getContentBounds() const override;
    BIND_METHOD(Pure = true)
    sf::Vector2f getOrigin() const override;
    BIND_METHOD()
    void setOrigin(const sf::Vector2f& origin) override;
    BIND_METHOD(Pure = true)
    sf::RenderStates getRenderStates() const override;
    BIND_METHOD()
    void update(float deltaTime) override;
    BIND_METHOD()
    void lateUpdate(float deltaTime) override;
    BIND_METHOD()
    void fixedUpdate(float fixedDelta) override;
    BIND_METHOD()
    void dispose();

    void refreshDisplayScale() override;
    void setTemplateFactory(
        std::function<std::shared_ptr<UiAssetInstance>()> factory);
    const std::vector<std::shared_ptr<UiAssetInstance>>& getInstances() const;
    void reflowItems();
    void applyPositions();
    void releaseRuntimeCallbacks() noexcept override;

protected:
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

private:
    void synchronizeCount();
    void clearItems();

    sf::Vector2f size_;
    sf::Vector2f spacing_;
    int count_ = 1;
    float displayScale_ = 1.0f;
    std::function<std::shared_ptr<UiAssetInstance>()> factory_;
    std::vector<std::shared_ptr<UiAssetInstance>> instances_;
};
