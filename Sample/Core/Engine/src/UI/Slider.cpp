#include <UI/Slider.hpp>

#include <Input/InputService.hpp>
#include <Runtime/EngineState.hpp>
#include <Utils/Math.hpp>

#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

std::optional<double> numericValue(const RuntimeValue& value) {
    if (const double* number = value.getIf<double>()) {
        return *number;
    }
    if (const std::int64_t* number = value.getIf<std::int64_t>()) {
        return static_cast<double>(*number);
    }
    return std::nullopt;
}

std::optional<sf::Vector2f> pointerPosition(
    const RuntimeValue::Map& arguments) {
    const auto positionIterator = arguments.find("position");
    if (positionIterator == arguments.end()) {
        return std::nullopt;
    }
    const RuntimeValue::Map* position =
        positionIterator->second.getIf<RuntimeValue::Map>();
    if (position == nullptr) {
        return std::nullopt;
    }
    const auto xIterator = position->find("x");
    const auto yIterator = position->find("y");
    if (xIterator == position->end() || yIterator == position->end()) {
        return std::nullopt;
    }
    const std::optional<double> x = numericValue(xIterator->second);
    const std::optional<double> y = numericValue(yIterator->second);
    if (!x.has_value() || !y.has_value()) {
        return std::nullopt;
    }
    return sf::Vector2f(static_cast<float>(*x), static_cast<float>(*y));
}

std::optional<sf::Mouse::Button> pointerButton(
    const RuntimeValue::Map& arguments) {
    const auto iterator = arguments.find("button");
    if (iterator == arguments.end()) {
        return std::nullopt;
    }
    const std::int64_t* value = iterator->second.getIf<std::int64_t>();
    if (value == nullptr) {
        return std::nullopt;
    }
    return static_cast<sf::Mouse::Button>(*value);
}

}  // namespace

Slider::Slider(const sf::Vector2f& size,
               std::shared_ptr<sf::Texture> lineTexture,
               std::shared_ptr<sf::Texture> handleTexture, int minValue,
               int maxValue, int value)
    : size_(normalizedSize(size)),
      lineTexture_(requireTexture(std::move(lineTexture))),
      handleTexture_(requireTexture(std::move(handleTexture))),
      line_(std::make_unique<sf::Sprite>(*lineTexture_)),
      handle_(std::make_unique<sf::Sprite>(*handleTexture_)),
      minValue_(std::min(minValue, maxValue)),
      maxValue_(std::max(minValue, maxValue)) {
    value_ = clampValue(value);
    setCanReceiveFocus(true);
    updateGeometry();
}

Slider::~Slider() = default;

void Slider::setVisible(bool visible) {
    ControlBase::setVisible(visible);
    if (!visible) {
        mouseDragging_ = false;
        touchDragging_ = false;
        suppressClick_ = false;
        resetPointerInteraction();
    }
}

sf::Vector2f Slider::getSize() const {
    return size_;
}

void Slider::resize(const sf::Vector2f& size) {
    const sf::Vector2f normalized = normalizedSize(size);
    if (size_ == normalized) {
        return;
    }
    size_ = normalized;
    updateGeometry();
}

int Slider::getValue() const {
    return value_;
}

void Slider::setValue(int value) {
    const int clamped = clampValue(value);
    if (value_ == clamped) {
        return;
    }
    value_ = clamped;
    updateGeometry();
    if (valueChangedCallback_) {
        valueChangedCallback_(value_);
    }
}

void Slider::setRange(int minValue, int maxValue) {
    const int normalizedMinimum = std::min(minValue, maxValue);
    const int normalizedMaximum = std::max(minValue, maxValue);
    if (minValue_ == normalizedMinimum && maxValue_ == normalizedMaximum) {
        return;
    }
    minValue_ = normalizedMinimum;
    maxValue_ = normalizedMaximum;
    value_ = clampValue(value_);
    updateGeometry();
}

std::pair<int, int> Slider::getRange() const {
    return {minValue_, maxValue_};
}

void Slider::setValueFromRatio(float ratio) {
    const float normalized =
        std::isfinite(ratio) ? std::clamp(ratio, 0.0f, 1.0f) : 0.0f;
    const double range =
        static_cast<double>(maxValue_) - static_cast<double>(minValue_);
    const double resolved = static_cast<double>(minValue_) +
                            static_cast<double>(normalized) * range;
    const std::int64_t rounded = roundNumber(resolved);
    setValue(static_cast<int>(std::clamp(
        rounded, static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max()))));
}

void Slider::setValueFromBoundsPosition(const sf::FloatRect& bounds,
                                        const sf::Vector2f& position) {
    if (!std::isfinite(bounds.size.x) || bounds.size.x <= 0.0f) {
        return;
    }
    setValueFromRatio((position.x - bounds.position.x) / bounds.size.x);
}

void Slider::adjust(int delta) {
    const long long adjusted =
        static_cast<long long>(value_) + static_cast<long long>(delta);
    setValue(static_cast<int>(std::clamp(
        adjusted, static_cast<long long>(std::numeric_limits<int>::min()),
        static_cast<long long>(std::numeric_limits<int>::max()))));
}

int Slider::getHandlePosition() const {
    const std::int64_t rounded = roundNumber(handleOffset());
    return static_cast<int>(std::clamp(
        rounded, static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

void Slider::setLineTexture(std::shared_ptr<sf::Texture> texture) {
    lineTexture_ = requireTexture(std::move(texture));
    line_->setTexture(*lineTexture_, true);
    updateGeometry();
}

void Slider::setHandleTexture(std::shared_ptr<sf::Texture> texture) {
    handleTexture_ = requireTexture(std::move(texture));
    handle_->setTexture(*handleTexture_, true);
    updateGeometry();
}

void Slider::setOnValueChanged(std::function<void(int)> callback) {
    valueChangedCallback_ = std::move(callback);
}

sf::FloatRect Slider::getLocalBounds() const {
    return {{0.0f, 0.0f}, size_};
}

void Slider::update(float deltaTime) {
    FunctionalInputProvider* provider = inputProvider();
    if (provider != nullptr && getVisible() && getActive() &&
        provider->isTouchBegan(false)) {
        const std::optional<sf::Vector2i> beganPosition =
            provider->getTouchBeganPosition();
        if (beganPosition.has_value() &&
            getAbsoluteBounds().contains(sf::Vector2f(*beganPosition))) {
            touchDragging_ = true;
            mouseDragging_ = false;
            suppressClick_ = false;
            requestKeyboardFocus();
            setValueFromBoundsPosition(getAbsoluteBounds(),
                                       sf::Vector2f(*beganPosition));
        }
    }
    FunctionalBase::update(deltaTime);
    updatePointerDrag();
}

void Slider::onClick(const RuntimeValue::Map& arguments) {
    const std::optional<sf::Vector2f> position = pointerPosition(arguments);
    if (!suppressClick_ && position.has_value()) {
        setValueFromBoundsPosition(getAbsoluteBounds(), *position);
    }
    suppressClick_ = false;
    FunctionalBase::onClick(arguments);
}

bool Slider::onMouseButtonDown(const RuntimeValue::Map& arguments) {
    const bool callbackHandled = FunctionalBase::onMouseButtonDown(arguments);
    const std::optional<sf::Mouse::Button> button = pointerButton(arguments);
    const std::optional<sf::Vector2f> position = pointerPosition(arguments);
    const bool accepted = button == sf::Mouse::Button::Left &&
                          position.has_value() &&
                          getAbsoluteBounds().contains(*position);
    suppressClick_ = !accepted;
    if (accepted) {
        mouseDragging_ = true;
        touchDragging_ = false;
        requestKeyboardFocus();
        setValueFromBoundsPosition(getAbsoluteBounds(), *position);
    }
    return callbackHandled || accepted;
}

void Slider::onMouseMoved(const RuntimeValue::Map& arguments) {
    const std::optional<sf::Vector2f> position = pointerPosition(arguments);
    if ((mouseDragging_ || touchDragging_) && position.has_value()) {
        setValueFromBoundsPosition(getAbsoluteBounds(), *position);
    }
    FunctionalBase::onMouseMoved(arguments);
}

void Slider::onKeyDown(const RuntimeValue::Map& arguments) {
    InputService* service = dynamic_cast<InputService*>(inputProvider());
    if (service != nullptr && ownsKeyboardCursorFocus()) {
        if (service->isActionTriggered(service->getLeftKeys(), true, 0.4f,
                                       0.05f)) {
            adjust(-1);
        }
        if (service->isActionTriggered(service->getRightKeys(), true, 0.4f,
                                       0.05f)) {
            adjust(1);
        }
    }
    FunctionalBase::onKeyDown(arguments);
}

void Slider::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    _applyRenderStates(states);
    if (!getVisible()) {
        return;
    }
    target.draw(*line_, states);
    target.draw(*handle_, states);
}

sf::Vector2f Slider::normalizedSize(const sf::Vector2f& size) {
    return {
        std::isfinite(size.x) ? std::max(2.0f, size.x) : 2.0f,
        std::isfinite(size.y) ? std::max(0.0f, size.y) : 0.0f,
    };
}

std::shared_ptr<sf::Texture> Slider::requireTexture(
    std::shared_ptr<sf::Texture> texture) {
    if (texture == nullptr) {
        throw std::invalid_argument("Slider texture must not be null");
    }
    return texture;
}

int Slider::clampValue(int value) const {
    return std::clamp(value, minValue_, maxValue_);
}

float Slider::valueRatio() const {
    if (maxValue_ == minValue_) {
        return 0.0f;
    }
    return static_cast<float>(
        (static_cast<double>(value_) - static_cast<double>(minValue_)) /
        (static_cast<double>(maxValue_) - static_cast<double>(minValue_)));
}

float Slider::handleWidth() const {
    return std::min(size_.x, static_cast<float>(handleTexture_->getSize().x));
}

float Slider::handleOffset() const {
    return valueRatio() * std::max(0.0f, size_.x - handleWidth());
}

void Slider::updateGeometry() {
    const sf::Vector2u lineTextureSize = lineTexture_->getSize();
    const float lineTextureWidth =
        static_cast<float>(std::max(1u, lineTextureSize.x));
    const float lineTextureHeight =
        static_cast<float>(std::max(1u, lineTextureSize.y));
    const float lineHeight = std::min(size_.y, lineTextureHeight);
    line_->setScale({
        size_.x * Scale / lineTextureWidth,
        lineHeight * Scale / lineTextureHeight,
    });
    line_->setPosition({
        0.0f,
        (size_.y - lineHeight) * 0.5f * Scale,
    });

    const sf::Vector2u handleTextureSize = handleTexture_->getSize();
    const float handleTextureWidth =
        static_cast<float>(std::max(1u, handleTextureSize.x));
    const float handleTextureHeight =
        static_cast<float>(std::max(1u, handleTextureSize.y));
    const float logicalHandleWidth = handleWidth();
    handle_->setScale({
        logicalHandleWidth * Scale / handleTextureWidth,
        size_.y * Scale / handleTextureHeight,
    });
    handle_->setPosition({handleOffset() * Scale, 0.0f});
}

void Slider::updatePointerDrag() {
    FunctionalInputProvider* provider = inputProvider();
    if (provider == nullptr || !getVisible() || !getActive()) {
        mouseDragging_ = false;
        touchDragging_ = false;
        return;
    }
    if (mouseDragging_) {
        setValueFromBoundsPosition(getAbsoluteBounds(),
                                   sf::Vector2f(provider->getMousePosition()));
        if ((provider->isMouseButtonReleased() &&
             provider->getMouseButtonReleased(sf::Mouse::Button::Left,
                                              false)) ||
            !provider->isMouseButtonDown(sf::Mouse::Button::Left)) {
            mouseDragging_ = false;
        }
    }
    if (touchDragging_) {
        const std::optional<sf::Vector2i> position =
            provider->isTouchEnded() ? provider->getTouchEndedPosition()
                                     : provider->getTouchPosition();
        if (position.has_value()) {
            setValueFromBoundsPosition(getAbsoluteBounds(),
                                       sf::Vector2f(*position));
        }
        if (provider->isTouchEnded() || !provider->isTouchActive()) {
            touchDragging_ = false;
        }
    }
}
