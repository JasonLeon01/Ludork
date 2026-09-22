#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <UI/ControlBase.hpp>
#include <UI/UiControlPropertyValue.hpp>

namespace ludork::engine::ui_asset_runtime_impl {
struct AssetImpl;
}

using UiAssetInstanceState = ludork::engine::ui_asset_runtime_impl::AssetImpl;
class UiAssetRuntime;

BIND_CLASS(name = "AssetInstance")
class LUDORK_ENGINE_API UiAssetInstance {
public:
    struct LUDORK_ENGINE_API UiAssetNodeView {
        std::string nodeName;
        std::shared_ptr<ControlBase> control;
        sf::FloatRect bounds;
        bool nestedBoundary = false;
        int zOrder = 0;
        std::size_t drawOrder = 0;
    };

    explicit UiAssetInstance(std::shared_ptr<UiAssetInstanceState> impl);

    virtual ~UiAssetInstance();

    BIND_METHOD(Pure = true)
    std::shared_ptr<ControlBase> getRoot() const;

    BIND_METHOD(Pure = true)
    std::shared_ptr<ControlBase> requireControl(
        const std::string& localName) const;

    BIND_METHOD(Pure = true)
    std::shared_ptr<ControlBase> getNodeByName(const std::string& name) const;

    BIND_METHOD(Pure = true)
    std::shared_ptr<UiAssetInstance> requireAsset(
        const std::string& localName) const;

    BIND_METHOD()
    void setProperty(const std::string& localName,
                     const std::string& propertyId,
                     const UiControlPropertyValue& value);

    BIND_METHOD()
    void setText(const std::string& localName, const std::string& text);

    BIND_METHOD(defaults = {nil})
    void reflow(std::optional<sf::Vector2u> logicalSize = std::nullopt);

    /// Resize an asset-local control and lay out its authored descendants.
    /// Its parent Slot and current position are unchanged; a parent reflow
    /// restores that Slot.
    BIND_METHOD()
    void reflowControl(const std::string& localName,
                       const sf::Vector2u& logicalSize);

    BIND_METHOD(Pure = true, defaults = {nil})
    bool hasAnimation(const std::string& name,
                      std::optional<std::string> target = std::nullopt) const;

    BIND_METHOD(defaults = {nil, nil})
    bool playAnimation(const std::string& name,
                       std::optional<std::string> target = std::nullopt,
                       std::function<void()> onFinished = {});

    BIND_METHOD(defaults = {nil})
    void stopAnimation(const std::string& name,
                       std::optional<std::string> target = std::nullopt);

    bool sampleAnimation(const std::string& name,
                         std::optional<std::string> target, float time);

    std::vector<UiAssetInstance::UiAssetNodeView> getNodeViews() const;

private:
    friend class UiAssetRuntime;
    std::shared_ptr<UiAssetInstanceState> impl_;
    std::unordered_map<std::string, std::shared_ptr<UiAssetInstance>>
        nestedAssets_;
};
