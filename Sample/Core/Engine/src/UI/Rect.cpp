#include <UI/Rect.hpp>

#include <Runtime/EngineState.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr float FallbackFadeSpeed = 96.0f;
constexpr float FallbackOpacityMin = 128.0f;
constexpr float FallbackOpacityMax = 255.0f;

}  // namespace

const std::string Rect::SelectionRectOpacityCurveKey =
    "UI/SelectionRectOpacity";

std::unordered_map<std::string, std::shared_ptr<Curve>> Rect::opacityCurves_;

void Rect::clearOpacityCurveCache() noexcept {
    opacityCurves_.clear();
}

Rect::Rect(const sf::IntRect& rect, const sf::Image& windowSkin,
           std::optional<std::string> opacityCurveKey)
    : SpriteBase(placeholderTexture()),
      size_(static_cast<float>(std::max(0, rect.size.x)),
            static_cast<float>(std::max(0, rect.size.y))),
      canvas_(std::make_shared<sf::RenderTexture>(
          nonZeroRenderTextureSize(sf::Vector2u(
              static_cast<unsigned int>(std::round(size_.x * Scale)),
              static_cast<unsigned int>(std::round(size_.y * Scale)))))),
      windowSkin_(windowSkin),
      opacityCurveKey_(opacityCurveKey.value_or(SelectionRectOpacityCurveKey)) {
    setPremultipliedTexture(true);
    initialiseUi();
    bindCanvasTexture();
    setPosition(sf::Vector2f(rect.position));
    opacity_ = static_cast<float>(getColour().a);
}

void Rect::setOpacityMultiplier(float multiplier) {
    opacityMultiplier_ = std::clamp(multiplier, 0.0f, 1.0f);
    applyOpacity();
}

void Rect::resize(const sf::Vector2f& size) {
    const sf::Vector2f logicalSize{
        std::max(0.0f, size.x),
        std::max(0.0f, size.y),
    };
    const sf::Vector2u logicalTextureSize{
        static_cast<unsigned int>(std::round(logicalSize.x * Scale)),
        static_cast<unsigned int>(std::round(logicalSize.y * Scale)),
    };
    const sf::Vector2u backingTextureSize =
        nonZeroRenderTextureSize(logicalTextureSize);
    if (size_ == logicalSize && canvas_->getSize() == backingTextureSize) {
        return;
    }
    size_ = logicalSize;
    if (!canvas_->resize(backingTextureSize)) {
        throw std::runtime_error(
            "Failed to resize selection rectangle render texture");
    }
    initialiseUi();
    bindCanvasTexture();
}

void Rect::setWindowSkin(const sf::Image& windowSkin) {
    windowSkin_ = windowSkin;
    initialiseUi();
    bindCanvasTexture();
}

sf::Vector2f Rect::getSize() const {
    return size_;
}

void Rect::update(float deltaTime) {
    const std::shared_ptr<Curve> curve = opacityCurve();
    if (curve != nullptr && !curve->keys.empty()) {
        const float duration = curve->getDuration();
        if (duration > 0.0f) {
            opacityTime_ = std::fmod(opacityTime_ + deltaTime, duration);
            opacity_ = curve->evaluate(curve->keys.front().time + opacityTime_);
        } else {
            opacity_ = curve->evaluate(curve->keys.front().time);
        }
    } else {
        updateFallbackOpacity(deltaTime);
    }
    applyOpacity();
}

void Rect::refreshDisplayScale() {
    resize(size_);
    ControlBase::refreshDisplayScale();
}

std::shared_ptr<sf::Texture> Rect::placeholderTexture() {
    return std::make_shared<sf::Texture>();
}

std::shared_ptr<Curve> Rect::resolveOpacityCurve(const std::string& key) {
    const auto cached = opacityCurves_.find(key);
    if (cached != opacityCurves_.end()) {
        return cached->second;
    }
    const std::vector<RuntimeValue> resolved =
        resolveRuntime("curve", {RuntimeValue(key)});
    if (resolved.empty()) {
        return nullptr;
    }
    const RuntimeValue::Object* object =
        resolved.front().getIf<RuntimeValue::Object>();
    if (object == nullptr) {
        return nullptr;
    }
    const std::shared_ptr<Curve> curve =
        std::dynamic_pointer_cast<Curve>(*object);
    if (curve != nullptr) {
        opacityCurves_.emplace(key, curve);
    }
    return curve;
}

void Rect::bindCanvasTexture() {
    std::shared_ptr<sf::Texture> texture(
        canvas_, const_cast<sf::Texture*>(&canvas_->getTexture()));
    setTexture(std::move(texture), true);
    const sf::Vector2u textureSize{
        static_cast<unsigned int>(std::round(size_.x * Scale)),
        static_cast<unsigned int>(std::round(size_.y * Scale)),
    };
    setTextureRect(
        {{0, 0},
         {static_cast<int>(textureSize.x), static_cast<int>(textureSize.y)}});
}

void Rect::initialiseUi() {
    cachedCornerTextures_.clear();
    cachedEdgeTextures_.clear();
    windowEdge_ = std::make_unique<sf::RenderTexture>(canvas_->getSize());
    windowBack_ = textureFromArea({{132, 68}, {24, 24}});
    windowEdgeSprite_ = std::make_unique<sf::Sprite>(windowEdge_->getTexture());
    windowBackSprite_ = std::make_unique<sf::Sprite>(*windowBack_);
    const sf::Vector2u canvasSize = canvas_->getSize();
    windowBackSprite_->setScale({static_cast<float>(canvasSize.x) / 24.0f,
                                 static_cast<float>(canvasSize.y) / 24.0f});

    const std::vector<sf::IntRect> corners = {
        {{128, 64}, {4, 4}},
        {{156, 64}, {4, 4}},
        {{128, 92}, {4, 4}},
        {{156, 92}, {4, 4}},
    };
    const std::vector<sf::IntRect> edges = {
        {{132, 64}, {24, 4}},
        {{132, 92}, {24, 4}},
        {{128, 68}, {4, 24}},
        {{156, 68}, {4, 24}},
    };
    std::vector<sf::Texture*> cachedCorners;
    std::vector<sf::Texture*> cachedEdges;
    cachedCorners.reserve(corners.size());
    cachedEdges.reserve(edges.size());
    for (const sf::IntRect& area : corners) {
        cachedCornerTextures_.push_back(textureFromArea(area));
        cachedCorners.push_back(cachedCornerTextures_.back().get());
    }
    for (const sf::IntRect& area : edges) {
        cachedEdgeTextures_.push_back(textureFromArea(area));
        cachedEdgeTextures_.back()->setRepeated(true);
        cachedEdges.push_back(cachedEdgeTextures_.back().get());
    }
    rectImpl_.render(*canvas_, *windowEdge_, *windowEdgeSprite_,
                     *windowBackSprite_, cachedCorners, cachedEdges,
                     canvasRenderStates());
}

std::unique_ptr<sf::Texture> Rect::textureFromArea(
    const sf::IntRect& area) const {
    return std::make_unique<sf::Texture>(windowSkin_, false, area);
}

std::shared_ptr<Curve> Rect::opacityCurve() const {
    return resolveOpacityCurve(opacityCurveKey_);
}

void Rect::updateFallbackOpacity(float deltaTime) {
    if (fading_) {
        opacity_ = std::max(opacity_ - FallbackFadeSpeed * deltaTime,
                            FallbackOpacityMin);
        if (opacity_ == FallbackOpacityMin) {
            fading_ = false;
        }
    } else {
        opacity_ = std::min(opacity_ + FallbackFadeSpeed * deltaTime,
                            FallbackOpacityMax);
        if (opacity_ == FallbackOpacityMax) {
            fading_ = true;
        }
    }
}

void Rect::applyOpacity() {
    sf::Color colour = getColour();
    const float alpha =
        std::floor(std::clamp(opacity_ * opacityMultiplier_, 0.0f, 255.0f));
    colour.a = static_cast<std::uint8_t>(alpha);
    setColour(colour);
}
