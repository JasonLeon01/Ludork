#include <UI/CheckBox.hpp>

#include "Interaction/InputArguments.hpp"

#include <Input/InputService.hpp>
#include <UI/Rect.hpp>

#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace {

using ludork::engine::ui_interaction::pointerMouseButton;
using ludork::engine::ui_interaction::pointerPosition;

}  // namespace

CheckBox::CheckBox(const sf::Vector2f& size, const sf::Image& windowSkin,
                   std::shared_ptr<PlainTextConfig> textConfig, bool checked)
    : size_(normalizedSize(size)),
      frame_(std::make_unique<Rect>(
          sf::IntRect({0, 0}, {static_cast<int>(integerSize(size_).x),
                               static_cast<int>(integerSize(size_).y)}),
          windowSkin)),
      checked_(checked) {
    setCanReceiveFocus(true);
    rebuildMark(std::move(textConfig));
}

CheckBox::~CheckBox() = default;

sf::Vector2f CheckBox::getSize() const {
    return size_;
}

void CheckBox::resize(const sf::Vector2f& size) {
    const sf::Vector2f normalized = normalizedSize(size);
    if (size_ == normalized) {
        return;
    }
    size_ = normalized;
    frame_->resize(size_);
    updateMark();
}

bool CheckBox::isChecked() const {
    return checked_;
}

void CheckBox::setChecked(bool checked) {
    if (checked_ == checked) {
        return;
    }
    checked_ = checked;
    updateMark();
    if (checkedChangedCallback_) {
        checkedChangedCallback_(checked_);
    }
}

void CheckBox::toggle() {
    setChecked(!checked_);
}

void CheckBox::setWindowSkin(const sf::Image& windowSkin) {
    frame_->setWindowSkin(windowSkin);
}

void CheckBox::setTextConfig(std::shared_ptr<PlainTextConfig> textConfig) {
    rebuildMark(std::move(textConfig));
}

void CheckBox::setOnCheckedChanged(std::function<void(bool)> callback) {
    checkedChangedCallback_ = std::move(callback);
}

sf::FloatRect CheckBox::getLocalBounds() const {
    return {{0.0f, 0.0f}, size_};
}

void CheckBox::update(float deltaTime) {
    FunctionalInputProvider* provider = inputProvider();
    if (provider != nullptr && isInteractionEnabled() &&
        provider->isTouchBegan(false)) {
        const std::optional<sf::Vector2i> beganPosition =
            provider->getTouchBeganPosition();
        if (beganPosition.has_value() &&
            getAbsoluteBounds().contains(sf::Vector2f(*beganPosition))) {
            suppressClick_ = false;
            requestKeyboardFocus();
        }
    }
    FunctionalBase::update(deltaTime);
}

void CheckBox::onConfirm(const RuntimeValue::Map& arguments) {
    toggle();
    FunctionalBase::onConfirm(arguments);
}

void CheckBox::onClick(const RuntimeValue::Map& arguments) {
    if (!suppressClick_) {
        toggle();
    }
    suppressClick_ = false;
    FunctionalBase::onClick(arguments);
}

bool CheckBox::onMouseButtonDown(const RuntimeValue::Map& arguments) {
    const bool callbackHandled = FunctionalBase::onMouseButtonDown(arguments);
    const std::optional<sf::Mouse::Button> button =
        pointerMouseButton(arguments);
    const std::optional<sf::Vector2f> position = pointerPosition(arguments);
    const bool accepted = button == sf::Mouse::Button::Left &&
                          position.has_value() &&
                          getAbsoluteInteractionBounds().contains(*position);
    suppressClick_ = !accepted;
    if (accepted) {
        requestKeyboardFocus();
    }
    return callbackHandled || accepted;
}

void CheckBox::onKeyDown(const RuntimeValue::Map& arguments) {
    InputService* service = dynamic_cast<InputService*>(inputProvider());
    if (service != nullptr && ownsKeyboardCursorFocus() &&
        service->isActionTriggered(service->getConfirmKeys(), true)) {
        onConfirm({});
    }
    FunctionalBase::onKeyDown(arguments);
}

void CheckBox::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    _applyRenderStates(states);
    if (!getVisible()) {
        return;
    }
    target.draw(*frame_, states);
    target.draw(*mark_, states);
}

void CheckBox::_refreshPresentationColour() {
    applyPresentationColour();
}

sf::Vector2f CheckBox::normalizedSize(const sf::Vector2f& size) {
    return {
        std::isfinite(size.x) ? std::round(std::max(0.0f, size.x)) : 0.0f,
        std::isfinite(size.y) ? std::round(std::max(0.0f, size.y)) : 0.0f,
    };
}

sf::Vector2u CheckBox::integerSize(const sf::Vector2f& size) {
    return {
        static_cast<unsigned int>(size.x),
        static_cast<unsigned int>(size.y),
    };
}

void CheckBox::rebuildMark(std::shared_ptr<PlainTextConfig> textConfig) {
    mark_ = std::make_unique<PlainText>(std::move(textConfig),
                                        checked_ ? "\xE2\x88\x9A" : "");
    mark_->setColour(checked_ ? sf::Color(0, 255, 0) : sf::Color::White);
    updateMark();
    applyPresentationColour();
}

void CheckBox::updateMark() {
    mark_->setString(checked_ ? "\xE2\x88\x9A" : "");
    mark_->setColour(checked_ ? sf::Color(0, 255, 0) : sf::Color::White);
    const sf::FloatRect bounds = mark_->getLocalBounds();
    mark_->setOrigin({0.0f, 0.0f});
    mark_->setPosition({
        (size_.x - bounds.size.x) / 2.0f - bounds.position.x,
        (size_.y - bounds.size.y) / 2.0f,
    });
}

void CheckBox::applyPresentationColour() {
    frame_->setPresentationColour(this, presentationColour());
    mark_->setPresentationColour(this, presentationColour());
}

void CheckBox::refreshDisplayScale() {
    frame_->refreshDisplayScale();
    mark_->refreshDisplayScale();
    updateMark();
    ControlBase::refreshDisplayScale();
}

void CheckBox::releaseRuntimeCallbacks() noexcept {
    checkedChangedCallback_ = {};
    ControlBase::releaseRuntimeCallbacks();
}
