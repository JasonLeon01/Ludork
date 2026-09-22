#include <UI/UiAssetInstance.hpp>

#include "UiAssets/AnimationImpl.hpp"
#include "UiAssets/NodeViewCollector.hpp"
#include "UiAssets/AssetImpl.hpp"

#include "UiControlAdapters/UiControlAdapterSupport.hpp"
#include <UI/UiControlAdapterRegistry.hpp>
#include <UI/UiLayoutEngine.hpp>

#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace {

void collectControlsByName(const std::shared_ptr<ControlBase>& control,
                           const std::string& name,
                           std::unordered_set<const ControlBase*>& visited,
                           std::vector<std::shared_ptr<ControlBase>>& result) {
    if (control == nullptr || !visited.insert(control.get()).second) {
        return;
    }
    if (control->getName() == name) {
        result.push_back(control);
    }
    for (const std::shared_ptr<ControlBase>& child : control->getChildren()) {
        collectControlsByName(child, name, visited, result);
    }
}

}  // namespace

UiAssetInstance::UiAssetInstance(std::shared_ptr<UiAssetInstanceState> impl)
    : impl_(std::move(impl)) {
    if (impl_ == nullptr || impl_->root == nullptr) {
        throw std::invalid_argument(
            "UI asset instance state must not be empty");
    }
    for (const auto& [name, nestedImpl] : impl_->nestedImpls) {
        nestedAssets_.emplace(name, std::shared_ptr<UiAssetInstance>(
                                        new UiAssetInstance(nestedImpl)));
    }
}

UiAssetInstance::~UiAssetInstance() {
    ludork::engine::ui_asset_runtime_impl::stopAllAnimations(impl_);
}

std::shared_ptr<ControlBase> UiAssetInstance::getRoot() const {
    return impl_->root->control;
}

std::shared_ptr<ControlBase> UiAssetInstance::requireControl(
    const std::string& localName) const {
    const auto iterator = impl_->controls.find(localName);
    if (iterator != impl_->controls.end()) {
        return iterator->second->control;
    }
    if (nestedAssets_.contains(localName)) {
        throw std::invalid_argument(
            localName + " is a nested UI asset; use requireAsset instead");
    }
    throw std::out_of_range("UI control not found in " + impl_->assetKey +
                            ": " + localName);
}

std::shared_ptr<ControlBase> UiAssetInstance::getNodeByName(
    const std::string& name) const {
    if (name.empty()) {
        throw std::invalid_argument("UI node name cannot be empty");
    }
    std::vector<std::shared_ptr<ControlBase>> matches;
    std::unordered_set<const ControlBase*> visited;
    collectControlsByName(impl_->root->control, name, visited, matches);
    if (matches.empty()) {
        return nullptr;
    }
    if (matches.size() != 1) {
        throw std::invalid_argument("UI node name is ambiguous in " +
                                    impl_->assetKey + ": " + name);
    }
    return matches.front();
}

std::shared_ptr<UiAssetInstance> UiAssetInstance::requireAsset(
    const std::string& localName) const {
    const auto iterator = nestedAssets_.find(localName);
    if (iterator == nestedAssets_.end()) {
        throw std::out_of_range("Nested UI asset not found in " +
                                impl_->assetKey + ": " + localName);
    }
    return iterator->second;
}

void UiAssetInstance::setProperty(const std::string& localName,
                                  const std::string& propertyId,
                                  const UiControlPropertyValue& value) {
    const auto iterator = impl_->controls.find(localName);
    if (iterator == impl_->controls.end()) {
        if (nestedAssets_.contains(localName)) {
            throw std::invalid_argument(
                "Nested UI asset properties cannot be overridden: " +
                localName);
        }
        throw std::out_of_range("UI control not found in " + impl_->assetKey +
                                ": " + localName);
    }
    UiRuntimeNode& node = *iterator->second;
    if (propertyId == "visible") {
        node.control->setVisible(
            ui_control_adapter_detail::requireBool(value, propertyId));
    } else if (propertyId == "rotation") {
        node.control->setRotationDegrees(
            ui_control_adapter_detail::requireFloat(value, propertyId));
    } else if (propertyId == "scale") {
        const sf::Vector2f scale =
            ui_control_adapter_detail::requireVector2f(value, propertyId);
        if (scale.x < 0.0f || scale.y < 0.0f) {
            throw std::invalid_argument("scale cannot be negative");
        }
        node.control->setScale(scale);
        node.renderScale = scale;
    } else if (propertyId == "origin") {
        node.control->setOrigin(
            ui_control_adapter_detail::requireVector2f(value, propertyId));
    } else {
        UiControlAdapterRegistry::instance().setProperty(
            node.controlId, *node.control, propertyId, value);
    }
    impl_->layoutDirty = true;
}

void UiAssetInstance::setText(const std::string& localName,
                              const std::string& text) {
    const auto iterator = impl_->controls.find(localName);
    if (iterator == impl_->controls.end()) {
        if (nestedAssets_.contains(localName)) {
            throw std::invalid_argument(
                "Nested UI asset text cannot be overridden: " + localName);
        }
        throw std::out_of_range("UI control not found in " + impl_->assetKey +
                                ": " + localName);
    }
    UiRuntimeNode& node = *iterator->second;
    if (!UiControlAdapterRegistry::instance().supportsProperty(node.controlId,
                                                               "text")) {
        throw std::invalid_argument(localName + " is not a text control");
    }
    UiControlAdapterRegistry::instance().setProperty(
        node.controlId, *node.control, "text", UiControlPropertyValue(text));
    impl_->layoutDirty = true;
}

void UiAssetInstance::reflow(std::optional<sf::Vector2u> logicalSize) {
    if (logicalSize.has_value()) {
        if (logicalSize->x == 0 || logicalSize->y == 0) {
            throw std::invalid_argument(
                "UI asset logical size must be positive");
        }
        impl_->logicalSize = {static_cast<float>(logicalSize->x),
                              static_cast<float>(logicalSize->y)};
    }
    UiLayoutEngine::reflow(*impl_, impl_->logicalSize);
}

void UiAssetInstance::reflowControl(const std::string& localName,
                                    const sf::Vector2u& logicalSize) {
    if (logicalSize.x == 0 || logicalSize.y == 0) {
        throw std::invalid_argument("UI control logical size must be positive");
    }
    UiLayoutEngine::reflowControl(
        *impl_, localName,
        {static_cast<float>(logicalSize.x), static_cast<float>(logicalSize.y)});
}

bool UiAssetInstance::hasAnimation(const std::string& name,
                                   std::optional<std::string> target) const {
    return ludork::engine::ui_asset_runtime_impl::hasAnimation(impl_, name,
                                                               target);
}

bool UiAssetInstance::playAnimation(const std::string& name,
                                    std::optional<std::string> target,
                                    std::function<void()> onFinished) {
    return ludork::engine::ui_asset_runtime_impl::playAnimation(
        impl_, name, target, std::move(onFinished));
}

void UiAssetInstance::stopAnimation(const std::string& name,
                                    std::optional<std::string> target) {
    ludork::engine::ui_asset_runtime_impl::stopAnimation(impl_, name, target);
}

bool UiAssetInstance::sampleAnimation(const std::string& name,
                                      std::optional<std::string> target,
                                      float time) {
    return ludork::engine::ui_asset_runtime_impl::sampleAnimation(impl_, name,
                                                                  target, time);
}

std::vector<UiAssetInstance::UiAssetNodeView> UiAssetInstance::getNodeViews()
    const {
    std::vector<UiAssetInstance::UiAssetNodeView> result;
    const auto views =
        ludork::engine::ui_asset_runtime_impl::collectNodeViews(impl_->root);
    result.reserve(views.size());
    for (const auto& view : views) {
        result.push_back({view.nodeName, view.control, view.bounds,
                          view.nestedBoundary, view.zOrder, view.drawOrder});
    }
    return result;
}
