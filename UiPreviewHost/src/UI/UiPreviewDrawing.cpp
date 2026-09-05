#include "UI/UiPreviewDrawing.hpp"

#include "Protocol/PreviewProtocol.hpp"
#include "Rendering/PixelConversion.hpp"

#include <UI/Canvas.hpp>
#include <UI/UiAssetRuntime.hpp>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::preview_host {
namespace {

constexpr float minimumRenderScale = 0.01f;

sf::Vector2u scaledRenderSize(const sf::Vector2u& design, float renderScale) {
    const float width = static_cast<float>(design.x) * renderScale;
    const float height = static_cast<float>(design.y) * renderScale;
    if (!std::isfinite(width) || !std::isfinite(height) || width < 1.0f ||
        height < 1.0f) {
        return {};
    }
    return {
        static_cast<unsigned int>(width),
        static_cast<unsigned int>(height),
    };
}

void renderNestedCanvases(const std::shared_ptr<ControlBase>& control) {
    for (const std::shared_ptr<ControlBase>& child : control->getChildren()) {
        if (child != nullptr && child->getVisible()) {
            renderNestedCanvases(child);
        }
    }
    const std::shared_ptr<Canvas> canvas =
        std::dynamic_pointer_cast<Canvas>(control);
    if (canvas != nullptr) {
        canvas->render();
    }
}

bool effectiveVisible(const std::shared_ptr<ControlBase>& control) {
    std::shared_ptr<ControlBase> current = control;
    while (current != nullptr) {
        if (!current->getVisible()) {
            return false;
        }
        current = current->getParent();
    }
    return true;
}

sf::FloatRect effectiveClip(const std::shared_ptr<ControlBase>& control,
                            const sf::FloatRect& rootClip) {
    sf::FloatRect clip = rootClip;
    std::shared_ptr<ControlBase> current = control;
    while (current != nullptr) {
        if (std::dynamic_pointer_cast<Canvas>(current) != nullptr) {
            const std::optional<sf::FloatRect> intersection =
                clip.findIntersection(current->getAbsoluteBounds());
            if (!intersection.has_value()) {
                return {{0.0f, 0.0f}, {0.0f, 0.0f}};
            }
            clip = *intersection;
        }
        current = current->getParent();
    }
    return clip;
}

bool insideEffectiveClip(const std::shared_ptr<ControlBase>& control,
                         const sf::Vector2f& point,
                         const sf::FloatRect& rootClip, float renderScale) {
    if (!rootClip.contains(point)) {
        return false;
    }
    std::shared_ptr<ControlBase> current = control;
    while (current != nullptr) {
        if (std::dynamic_pointer_cast<Canvas>(current) != nullptr) {
            const sf::Vector2f local =
                current->screenRenderTransform().getInverse().transformPoint(
                    point) /
                renderScale;
            if (!current->getLocalBounds().contains(local)) {
                return false;
            }
        }
        current = current->getParent();
    }
    return true;
}

}  // namespace

RenderTargetSpec renderTargetSpec(const sf::Vector2u& design,
                                  double requestedScale) {
    if (requestedScale < static_cast<double>(minimumRenderScale)) {
        throw std::invalid_argument(
            "Render request.renderScale is below the engine minimum");
    }
    const unsigned int maximumTextureSize = sf::Texture::getMaximumSize();
    if (maximumTextureSize == 0) {
        throw std::runtime_error(
            "OpenGL does not report a usable maximum texture size");
    }
    const double maximumScale = std::min(
        static_cast<double>(maximumTextureSize) / static_cast<double>(design.x),
        static_cast<double>(maximumTextureSize) /
            static_cast<double>(design.y));
    if (maximumScale < static_cast<double>(minimumRenderScale)) {
        throw std::invalid_argument(
            "UI asset designSize cannot fit the maximum texture size");
    }
    float renderScale =
        static_cast<float>(std::min(requestedScale, maximumScale));
    sf::Vector2u size = scaledRenderSize(design, renderScale);
    while ((size.x > maximumTextureSize || size.y > maximumTextureSize) &&
           renderScale > minimumRenderScale) {
        renderScale = std::nextafter(renderScale, 0.0f);
        size = scaledRenderSize(design, renderScale);
    }
    if (renderScale < minimumRenderScale || size.x == 0 || size.y == 0 ||
        size.x > maximumTextureSize || size.y > maximumTextureSize) {
        throw std::invalid_argument(
            "Render request.renderScale cannot produce a valid preview");
    }
    return {renderScale, size};
}

std::vector<std::uint8_t> renderFrame(
    const std::shared_ptr<UiAssetInstance>& instance,
    const sf::Vector2u& size) {
    const std::shared_ptr<ControlBase> root = instance->getRoot();
    sf::RenderTexture target(size);
    target.clear(sf::Color::Transparent);
    if (root->getVisible()) {
        renderNestedCanvases(root);
        target.draw(*root);
    }
    target.display();
    const sf::Image image = target.getTexture().copyToImage();
    return bgraFromPremultipliedRgba(image, size);
}

RuntimeData::Array nodeGeometry(
    const std::shared_ptr<UiAssetInstance>& instance, const sf::Vector2u& size,
    float renderScale) {
    const sf::FloatRect rootClip(
        {0.0f, 0.0f}, {static_cast<float>(size.x), static_cast<float>(size.y)});
    RuntimeData::Array result;
    for (const UiAssetNodeView& node : instance->getNodeViews()) {
        const sf::FloatRect clip = effectiveClip(node.control, rootClip);
        result.emplace_back(object({
            {"nodeName", RuntimeData(node.nodeName)},
            {"x", number(node.bounds.position.x / renderScale)},
            {"y", number(node.bounds.position.y / renderScale)},
            {"width", number(node.bounds.size.x / renderScale)},
            {"height", number(node.bounds.size.y / renderScale)},
            {"clipX", number(clip.position.x / renderScale)},
            {"clipY", number(clip.position.y / renderScale)},
            {"clipWidth", number(clip.size.x / renderScale)},
            {"clipHeight", number(clip.size.y / renderScale)},
            {"drawOrder",
             RuntimeData(static_cast<std::int64_t>(node.drawOrder))},
            {"visible", RuntimeData(effectiveVisible(node.control))},
        }));
    }
    return result;
}

std::optional<std::string> hitTestUiPreview(
    const std::shared_ptr<UiAssetInstance>& instance,
    const sf::Vector2u& renderSize, float renderScale,
    const sf::Vector2f& logicalPoint) {
    const sf::Vector2f point = logicalPoint * renderScale;
    const sf::FloatRect rootClip(
        {0.0f, 0.0f},
        {static_cast<float>(renderSize.x), static_cast<float>(renderSize.y)});
    std::vector<UiAssetNodeView> nodes = instance->getNodeViews();
    std::stable_sort(
        nodes.begin(), nodes.end(),
        [](const UiAssetNodeView& left, const UiAssetNodeView& right) {
            return left.drawOrder > right.drawOrder;
        });
    for (const UiAssetNodeView& node : nodes) {
        if (!effectiveVisible(node.control)) {
            continue;
        }
        if (!node.bounds.contains(point)) {
            continue;
        }
        if (!insideEffectiveClip(node.control, point, rootClip, renderScale)) {
            continue;
        }
        const sf::Vector2f local =
            node.control->screenRenderTransform().getInverse().transformPoint(
                point) /
            renderScale;
        if (node.control->getLocalBounds().contains(local)) {
            return node.nodeName;
        }
    }
    return std::nullopt;
}

}  // namespace ludork::preview_host
