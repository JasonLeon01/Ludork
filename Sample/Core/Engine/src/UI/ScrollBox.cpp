#include <UI/ScrollBox.hpp>

#include "Interaction/InputArguments.hpp"

#include <Input/InputService.hpp>
#include <Runtime/EngineState.hpp>
#include <Runtime/RuntimeValueReader.hpp>
#include <UI/DropBox.hpp>
#include <UI/Slider.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

constexpr sf::IntRect UpIndicatorRect{{156, 16}, {8, 12}};
constexpr sf::IntRect DownIndicatorRect{{156, 36}, {8, 12}};
constexpr sf::IntRect LeftIndicatorRect{{144, 28}, {12, 8}};
constexpr sf::IntRect RightIndicatorRect{{164, 28}, {12, 8}};
constexpr sf::Vector2u MinimumWindowSkinSize{176, 48};

float clampAxis(float value, float maximum) {
    return std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, maximum);
}

sf::Vector2f clampedOffset(const sf::Vector2f& value,
                           const sf::Vector2f& maximum) {
    return {clampAxis(value.x, maximum.x), clampAxis(value.y, maximum.y)};
}

}  // namespace

ScrollBox::ScrollBox(const sf::Vector2f& size, const sf::Image& windowSkin)
    : Canvas(sf::IntRect({0, 0}, sf::Vector2i(roundedSize(size)))) {
    rebuildIndicators(windowSkin);
}

sf::Vector2f ScrollBox::getSize() const {
    return Canvas::getSize();
}

void ScrollBox::resize(const sf::Vector2f& size) {
    Canvas::resize(roundedSize(size));
    clampOffsets();
    applyView();
}

void ScrollBox::setWindowSkin(const sf::Image& windowSkin) {
    rebuildIndicators(windowSkin);
}

sf::Vector2f ScrollBox::getScrollOffset() const {
    return scrollOffset_;
}

void ScrollBox::setScrollOffset(const sf::Vector2f& offset) {
    scrollTargetOffset_.reset();
    scrollOffset_ = clampedOffset(offset, getMaxScrollOffset());
    applyView();
}

sf::Vector2f ScrollBox::getMaxScrollOffset() const {
    const sf::FloatRect content = aggregateContentBounds();
    const sf::Vector2f size = getSize();
    return {std::max(0.0f, content.position.x + content.size.x - size.x),
            std::max(0.0f, content.position.y + content.size.y - size.y)};
}

bool ScrollBox::getScrollingEnabled() const {
    return scrollingEnabled_;
}

void ScrollBox::setScrollingEnabled(bool enabled) {
    scrollingEnabled_ = enabled;
    if (!scrollingEnabled_) {
        scrollTargetOffset_.reset();
        resetPointerInteraction();
    }
}

void ScrollBox::scrollDescendantIntoView(
    const std::shared_ptr<ControlBase>& descendant) {
    if (descendant == nullptr || !isDescendant(descendant)) {
        throw std::invalid_argument(
            "ScrollBox descendant must belong to this ScrollBox");
    }
    const sf::FloatRect viewport = getAbsoluteBounds();
    const sf::FloatRect bounds = descendant->getAbsoluteBounds();
    sf::Vector2f offset = scrollOffset_;
    const float scale = std::max(Scale, 0.000001f);
    if (bounds.position.x < viewport.position.x) {
        offset.x -= (viewport.position.x - bounds.position.x) / scale;
    } else if (bounds.position.x + bounds.size.x >
               viewport.position.x + viewport.size.x) {
        offset.x += (bounds.position.x + bounds.size.x - viewport.position.x -
                     viewport.size.x) /
                    scale;
    }
    if (bounds.position.y < viewport.position.y) {
        offset.y -= (viewport.position.y - bounds.position.y) / scale;
    } else if (bounds.position.y + bounds.size.y >
               viewport.position.y + viewport.size.y) {
        offset.y += (bounds.position.y + bounds.size.y - viewport.position.y -
                     viewport.size.y) /
                    scale;
    }
    setScrollOffset(offset);
}

void ScrollBox::update(float deltaTime) {
    clampOffsets();
    updateWheelScroll(deltaTime);
    Canvas::update(deltaTime);
    updateTouchArbitration();
}

void ScrollBox::render() {
    clampOffsets();
    Canvas::render();
    drawIndicators();
}

void ScrollBox::onMouseMoved(const RuntimeValue::Map& arguments) {
    if (hasTouchCapture() && inputService().isTouchDragged()) {
        const std::optional<sf::Vector2f> position =
            ludork::engine::ui_interaction::pointerPosition(arguments);
        if (position.has_value()) {
            applyTouchScroll(*position);
        }
    }
    FunctionalBase::onMouseMoved(arguments);
}

void ScrollBox::onMouseWheelScrolled(const RuntimeValue::Map& arguments) {
    if (!scrollingEnabled_) {
        FunctionalBase::onMouseWheelScrolled(arguments);
        return;
    }
    const auto iterator = arguments.find("delta");
    if (iterator == arguments.end()) {
        FunctionalBase::onMouseWheelScrolled(arguments);
        return;
    }
    const double delta = ludork::engine::runtime_value_reader::requireNumber(
        iterator->second, "delta");
    if (delta == 0.0) {
        FunctionalBase::onMouseWheelScrolled(arguments);
        return;
    }
    InputService& input = inputService();
    const std::optional<sf::Mouse::Wheel> wheel = input.getMouseScrolledWheel();
    const bool horizontal =
        wheel.has_value() && *wheel == sf::Mouse::Wheel::Horizontal;
    sf::Vector2f offset = scrollTargetOffset_.value_or(scrollOffset_);
    const float distance =
        input.isMouseWheelPrecise()
            ? static_cast<float>(delta) / std::max(Scale, 0.000001f)
            : static_cast<float>(delta) * WheelStep;
    if (horizontal) {
        offset.x -= distance;
    } else {
        offset.y -= distance;
    }
    if (input.isMouseWheelPrecise()) {
        setScrollOffset(offset);
    } else {
        setScrollTargetOffset(offset);
    }
    FunctionalBase::onMouseWheelScrolled(arguments);
}

bool ScrollBox::acceptsTouchCapture() const {
    return scrollingEnabled_ && getMaxScrollOffset() != sf::Vector2f{};
}

void ScrollBox::onTouchCaptureBegan(const sf::Vector2f& position) {
    touchStartPosition_ = position;
    touchStartOffset_ = scrollOffset_;
    scrollTargetOffset_.reset();
}

void ScrollBox::onPointerInteractionReset() {
    touchStartPosition_ = {};
    touchStartOffset_ = scrollOffset_;
    touchArbitratedChild_.reset();
    touchArbitrationActive_ = false;
    touchScrollOwned_ = false;
    touchChildOwned_ = false;
    FunctionalBase::onPointerInteractionReset();
}

std::optional<sf::FloatRect> ScrollBox::_getAbsoluteChildInteractionClipBounds()
    const {
    return getAbsoluteBounds();
}

sf::FloatRect ScrollBox::getAbsoluteBounds() const {
    const sf::FloatRect bounds = getLocalBounds();
    const sf::FloatRect scaledBounds(bounds.position * Scale,
                                     bounds.size * Scale);
    return ControlBase::_getScreenRenderTransform().transformRect(scaledBounds);
}

sf::Vector2u ScrollBox::roundedSize(const sf::Vector2f& size) {
    return {static_cast<unsigned int>(std::max(
                0.0f, std::round(std::isfinite(size.x) ? size.x : 0.0f))),
            static_cast<unsigned int>(std::max(
                0.0f, std::round(std::isfinite(size.y) ? size.y : 0.0f)))};
}

std::size_t ScrollBox::indicatorIndex(Indicator indicator) {
    return static_cast<std::size_t>(indicator);
}

sf::FloatRect ScrollBox::aggregateContentBounds() const {
    sf::FloatRect bounds{{0.0f, 0.0f}, getSize()};
    for (const std::shared_ptr<ControlBase>& child : getChildren()) {
        if (child == nullptr || !child->getVisible()) {
            continue;
        }
        const sf::FloatRect childBounds =
            child->getTransform().transformRect(child->getContentBounds());
        const float minimumX =
            std::min(bounds.position.x, childBounds.position.x);
        const float minimumY =
            std::min(bounds.position.y, childBounds.position.y);
        const float maximumX =
            std::max(bounds.position.x + bounds.size.x,
                     childBounds.position.x + childBounds.size.x);
        const float maximumY =
            std::max(bounds.position.y + bounds.size.y,
                     childBounds.position.y + childBounds.size.y);
        bounds = {{minimumX, minimumY},
                  {maximumX - minimumX, maximumY - minimumY}};
    }
    return bounds;
}

void ScrollBox::clampOffsets() {
    const sf::Vector2f maximum = getMaxScrollOffset();
    const sf::Vector2f clamped = clampedOffset(scrollOffset_, maximum);
    if (clamped != scrollOffset_) {
        scrollOffset_ = clamped;
        applyView();
    }
    if (scrollTargetOffset_.has_value()) {
        *scrollTargetOffset_ = clampedOffset(*scrollTargetOffset_, maximum);
    }
}

void ScrollBox::setScrollTargetOffset(const sf::Vector2f& offset) {
    scrollTargetOffset_ = clampedOffset(offset, getMaxScrollOffset());
}

void ScrollBox::updateWheelScroll(float deltaTime) {
    if (!scrollTargetOffset_.has_value()) {
        return;
    }
    const sf::Vector2f distance = *scrollTargetOffset_ - scrollOffset_;
    if (std::abs(distance.x) <= WheelEpsilon &&
        std::abs(distance.y) <= WheelEpsilon) {
        scrollOffset_ = *scrollTargetOffset_;
        scrollTargetOffset_.reset();
        applyView();
        return;
    }
    const float factor =
        1.0f - std::exp(-WheelResponse * std::max(0.0f, deltaTime));
    scrollOffset_ += distance * factor;
    applyView();
}

void ScrollBox::applyView() {
    const sf::Vector2f size = getSize();
    setView(sf::View(scrollOffset_ + size / 2.0f, size));
}

void ScrollBox::updateTouchArbitration() {
    InputService& input = inputService();
    if (!scrollingEnabled_) {
        touchArbitratedChild_.reset();
        touchArbitrationActive_ = false;
        touchScrollOwned_ = false;
        touchChildOwned_ = false;
        return;
    }
    if (!touchArbitrationActive_) {
        const std::optional<sf::Vector2i> beganPosition =
            input.getTouchBeganPosition();
        if (!input.isTouchActive() || !beganPosition.has_value() ||
            !getAbsoluteBounds().contains(sf::Vector2f(*beganPosition))) {
            return;
        }
        for (const std::shared_ptr<ControlBase>& child : getChildren()) {
            const std::shared_ptr<ControlBase> captured =
                findCapturedDescendant(child);
            if (captured != nullptr) {
                touchArbitratedChild_ = captured;
                touchArbitrationActive_ = true;
                touchStartPosition_ = sf::Vector2f(*beganPosition);
                touchStartOffset_ = scrollOffset_;
                break;
            }
        }
    }
    if (!touchArbitrationActive_) {
        return;
    }
    if (!input.isTouchActive()) {
        touchArbitratedChild_.reset();
        touchArbitrationActive_ = false;
        touchScrollOwned_ = false;
        touchChildOwned_ = false;
        return;
    }
    if (touchScrollOwned_ || touchChildOwned_ || !input.isTouchDragged() ||
        !input.isTouchMoved()) {
        return;
    }
    const std::shared_ptr<ControlBase> captured = touchArbitratedChild_.lock();
    const std::optional<sf::Vector2i> position = input.getTouchPosition();
    if (captured == nullptr || !position.has_value()) {
        return;
    }
    if (const DropBox* dropBox = dynamic_cast<const DropBox*>(captured.get());
        dropBox != nullptr && dropBox->isExpanded()) {
        touchChildOwned_ = true;
        return;
    }
    if (dynamic_cast<const ScrollBox*>(captured.get()) != nullptr) {
        touchChildOwned_ = true;
        return;
    }
    const sf::Vector2f delta = sf::Vector2f(*position) - touchStartPosition_;
    const float absoluteX = std::abs(delta.x);
    const float absoluteY = std::abs(delta.y);
    if (dynamic_cast<const Slider*>(captured.get()) != nullptr &&
        absoluteX > absoluteY) {
        touchChildOwned_ = true;
        return;
    }
    const sf::Vector2f maximum = getMaxScrollOffset();
    const bool vertical = absoluteY >= absoluteX;
    if ((vertical && maximum.y <= 0.0f) || (!vertical && maximum.x <= 0.0f)) {
        touchChildOwned_ = true;
        return;
    }
    FunctionalBase* functional = dynamic_cast<FunctionalBase*>(captured.get());
    if (functional == nullptr || !functional->hasTouchCapture()) {
        return;
    }
    functional->releaseTouchCaptureForScroll();
    beginTouchPress();
    onTouchCaptureBegan(touchStartPosition_);
    touchScrollOwned_ = true;
    applyTouchScroll(sf::Vector2f(*position));
}

void ScrollBox::applyTouchScroll(const sf::Vector2f& position) {
    const float scale = std::max(Scale, 0.000001f);
    const sf::Vector2f delta = (position - touchStartPosition_) / scale;
    const sf::Vector2f maximum = getMaxScrollOffset();
    sf::Vector2f offset = touchStartOffset_;
    if (maximum.x > 0.0f) {
        offset.x -= delta.x;
    }
    if (maximum.y > 0.0f) {
        offset.y -= delta.y;
    }
    setScrollOffset(offset);
}

std::shared_ptr<ControlBase> ScrollBox::findCapturedDescendant(
    const std::shared_ptr<ControlBase>& root) const {
    if (root == nullptr || !root->getVisible()) {
        return nullptr;
    }
    for (const std::shared_ptr<ControlBase>& child : root->getChildren()) {
        const std::shared_ptr<ControlBase> captured =
            findCapturedDescendant(child);
        if (captured != nullptr) {
            return captured;
        }
    }
    FunctionalBase* functional = dynamic_cast<FunctionalBase*>(root.get());
    if (functional != nullptr && functional->hasTouchCapture()) {
        return root;
    }
    return nullptr;
}

void ScrollBox::rebuildIndicators(const sf::Image& windowSkin) {
    const sf::Vector2u size = windowSkin.getSize();
    if (size.x < MinimumWindowSkinSize.x || size.y < MinimumWindowSkinSize.y) {
        throw std::invalid_argument(
            "ScrollBox window skin must contain the 144,16,32,32 indicator "
            "atlas");
    }
    const std::array rectangles = {UpIndicatorRect, DownIndicatorRect,
                                   LeftIndicatorRect, RightIndicatorRect};
    for (std::size_t index = 0; index < rectangles.size(); ++index) {
        indicatorTextures_[index] =
            std::make_unique<sf::Texture>(windowSkin, false, rectangles[index]);
        indicatorTextures_[index]->setSmooth(false);
        indicatorSprites_[index] =
            std::make_unique<sf::Sprite>(*indicatorTextures_[index]);
    }
}

void ScrollBox::drawIndicators() {
    const sf::Vector2f maximum = getMaxScrollOffset();
    const sf::Vector2f size = getSize();
    indicatorSprites_[indicatorIndex(Indicator::Up)]->setPosition(
        {(size.x - 8.0f) / 2.0f, 0.0f});
    indicatorSprites_[indicatorIndex(Indicator::Down)]->setPosition(
        {(size.x - 8.0f) / 2.0f, size.y - 12.0f});
    indicatorSprites_[indicatorIndex(Indicator::Left)]->setPosition(
        {0.0f, (size.y - 8.0f) / 2.0f});
    indicatorSprites_[indicatorIndex(Indicator::Right)]->setPosition(
        {size.x - 12.0f, (size.y - 8.0f) / 2.0f});

    sf::RenderTexture& target = getRenderTexture();
    const sf::View savedView = target.getView();
    target.setView(target.getDefaultView());
    sf::RenderStates states = canvasRenderStates();
    states.transform.scale({Scale, Scale});
    if (scrollOffset_.y > 0.0f) {
        target.draw(*indicatorSprites_[indicatorIndex(Indicator::Up)], states);
    }
    if (scrollOffset_.y < maximum.y) {
        target.draw(*indicatorSprites_[indicatorIndex(Indicator::Down)],
                    states);
    }
    if (scrollOffset_.x > 0.0f) {
        target.draw(*indicatorSprites_[indicatorIndex(Indicator::Left)],
                    states);
    }
    if (scrollOffset_.x < maximum.x) {
        target.draw(*indicatorSprites_[indicatorIndex(Indicator::Right)],
                    states);
    }
    target.display();
    target.setView(savedView);
}

bool ScrollBox::isDescendant(
    const std::shared_ptr<ControlBase>& control) const {
    std::shared_ptr<ControlBase> current = control;
    while (current != nullptr) {
        if (current.get() == this) {
            return true;
        }
        current = current->getParent();
    }
    return false;
}
