#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <UI/ControlBase.hpp>

#include <SFML/Graphics/Rect.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ludork::engine::ui_asset_runtime_impl {
struct AssetState;
}

using UiAssetInstanceState = ludork::engine::ui_asset_runtime_impl::AssetState;
class UiAssetRuntime;

struct LUDORK_ENGINE_API UiAssetNodeView {
    std::string nodeName;
    std::shared_ptr<ControlBase> control;
    sf::FloatRect bounds;
    bool nestedBoundary = false;
    int zOrder = 0;
    std::size_t drawOrder = 0;
};

BIND_CLASS(name = "AssetInstance")
class LUDORK_ENGINE_API UiAssetInstance {
public:
    explicit UiAssetInstance(std::shared_ptr<UiAssetInstanceState> state);

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
                     const std::string& propertyId, const RuntimeValue& value);

    BIND_METHOD()
    void setText(const std::string& localName, const std::string& text);

    BIND_METHOD(defaults = {nil})
    void reflow(std::optional<sf::Vector2u> logicalSize = std::nullopt);

    std::vector<UiAssetNodeView> getNodeViews() const;

private:
    friend class UiAssetRuntime;
    std::shared_ptr<UiAssetInstanceState> state_;
    std::unordered_map<std::string, std::shared_ptr<UiAssetInstance>>
        nestedAssets_;
};

class LUDORK_ENGINE_API UiAssetRuntime {
public:
    static UiAssetRuntime& instance();

    std::shared_ptr<UiAssetInstance> instantiate(
        const std::string& assetKey,
        std::optional<sf::Vector2u> logicalSize = std::nullopt) const;

    std::shared_ptr<UiAssetInstance> instantiateSnapshot(
        const std::string& assetKey, const RuntimeValue& asset,
        const RuntimeValue::Map& dependencies,
        std::optional<sf::Vector2u> logicalSize = std::nullopt,
        bool designMode = true) const;
};

BIND_FUNCTION(name = "instantiate", defaults = {nil})
LUDORK_ENGINE_API std::shared_ptr<UiAssetInstance> instantiateUiAsset(
    const std::string& assetKey,
    std::optional<sf::Vector2u> logicalSize = std::nullopt);
