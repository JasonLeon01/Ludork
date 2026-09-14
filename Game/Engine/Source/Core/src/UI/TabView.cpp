#include <UI/TabView.hpp>
#include <Input/InputNamedValue.hpp>
#include <Input/JoystickButton.hpp>
#include <UI/PlainText.hpp>
#include <UI/PlainTextConfig.hpp>

#include "Interaction/InputArguments.hpp"
#include "Interaction/JoystickState.hpp"
#include "Interaction/GamepadGlyphImpl.hpp"
#include "TabView/KeyHintImpl.hpp"
#include "TabView/NavigationImpl.hpp"
#include "TabView/VisualLayout.hpp"

#include <Input/InputService.hpp>
#include <EngineState.hpp>
#include <UI/Rect.hpp>
#include <UI/SolidRect.hpp>
#include <UI/UiAudioService.hpp>

#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {

constexpr float HintSize = 16.0f;
constexpr float HintContentSize = 14.0f;
constexpr unsigned int HintCharacterSize = 8;

std::shared_ptr<PlainTextConfig> hintTextConfig(
    const std::shared_ptr<PlainTextConfig>& source) {
    if (source == nullptr) {
        throw std::invalid_argument("TabView text config must not be null");
    }
    std::shared_ptr<PlainTextConfig> result =
        std::make_shared<PlainTextConfig>(*source);
    result->characterSize = HintCharacterSize;
    result->style = sf::Text::Regular;
    result->slantAngle = 0.0f;
    result->fillColor = sf::Color::White;
    result->outline.color = sf::Color::Transparent;
    result->outline.thickness = 0.0f;
    result->glow = {};
    result->gradient = {};
    return result;
}

}  // namespace

TabView::TabView(const sf::Vector2f& size, const sf::Image& windowSkin,
                 std::shared_ptr<PlainTextConfig> textConfig,
                 std::vector<std::string> items, int selectedIndex)
    : size_(normalizedSize(size)),
      windowSkin_(windowSkin),
      textConfig_(std::move(textConfig)),
      items_(std::move(items)) {
    if (textConfig_ == nullptr) {
        throw std::invalid_argument("TabView text config must not be null");
    }
    if (items_.empty()) {
        throw std::invalid_argument("TabView items must not be empty");
    }
    selectedIndex_ = clampedIndex(selectedIndex, items_.size());
    setCanReceiveFocus(false);
    rebuildVisuals();
}

TabView::~TabView() = default;

sf::Vector2f TabView::getSize() const {
    return size_;
}

void TabView::resize(const sf::Vector2f& size) {
    const sf::Vector2f normalized = normalizedSize(size);
    if (size_ == normalized) {
        return;
    }
    size_ = normalized;
    layoutVisuals();
}

void TabView::setWindowSkin(const sf::Image& windowSkin) {
    windowSkin_ = windowSkin;
    selectionRect_->setWindowSkin(windowSkin_);
}

void TabView::setTextConfig(std::shared_ptr<PlainTextConfig> textConfig) {
    if (textConfig == nullptr) {
        throw std::invalid_argument("TabView text config must not be null");
    }
    textConfig_ = std::move(textConfig);
    rebuildLabels();
    rebuildHintVisuals();
    layoutVisuals();
    applyPresentationColour();
}

std::vector<std::string> TabView::getItems() const {
    return items_;
}

void TabView::setItems(const std::vector<std::string>& items) {
    if (items.size() != items_.size()) {
        throw std::invalid_argument(
            "TabView item count cannot change after construction");
    }
    items_ = items;
    for (std::size_t index = 0; index < items_.size(); ++index) {
        labels_[index]->setString(items_[index]);
        layoutLabel(*labels_[index], static_cast<int>(index));
    }
}

void TabView::setOnSelectedIndexChanged(
    std::optional<std::function<void(int)>> callback) {
    selectedIndexChangedCallback_ = callback.has_value()
                                        ? std::move(*callback)
                                        : std::function<void(int)>();
}

int TabView::getSelectedIndex() const {
    return selectedIndex_;
}

void TabView::setSelectedIndex(int index) {
    setSelectedIndexInternal(index, false);
}

std::string TabView::getSelectedItem() const {
    return items_[static_cast<std::size_t>(selectedIndex_)];
}

bool TabView::selectPrevious() {
    return setSelectedIndexInternal(selectedIndex_ - 1, true);
}

bool TabView::selectNext() {
    return setSelectedIndexInternal(selectedIndex_ + 1, true);
}

bool TabView::handleNavigationInput() {
    if (!isInteractionEnabled()) {
        return false;
    }
    InputService* input = ludork::Cast<InputService>(inputProvider());
    if (input == nullptr) {
        return false;
    }

    const bool keyboardLeft = input->isKeyTriggered(sf::Keyboard::Key::Q, false,
                                                    false, false, false, false);
    const InputNamedValue leftButton = JoystickButton::getLB();
    const bool handleLeft =
        input->isAnyJoystickButtonValueTriggered(leftButton, false);
    const bool keyboardRight = input->isKeyTriggered(
        sf::Keyboard::Key::E, false, false, false, false, false);
    const InputNamedValue rightButton = JoystickButton::getRB();
    const bool handleRight =
        input->isAnyJoystickButtonValueTriggered(rightButton, false);
    if (!keyboardLeft && !handleLeft && !keyboardRight && !handleRight) {
        return false;
    }

    if (keyboardLeft) {
        input->isKeyTriggered(sf::Keyboard::Key::Q, false, false, false, false,
                              true);
    }
    if (handleLeft) {
        input->isAnyJoystickButtonValueTriggered(leftButton, true);
    }
    if (keyboardRight) {
        input->isKeyTriggered(sf::Keyboard::Key::E, false, false, false, false,
                              true);
    }
    if (handleRight) {
        input->isAnyJoystickButtonValueTriggered(rightButton, true);
    }

    const bool moveLeft = keyboardLeft || handleLeft;
    const bool moveRight = keyboardRight || handleRight;
    if (moveLeft == moveRight) {
        return true;
    }
    if (moveLeft) {
        setSelectedIndexInternal(selectedIndex_ - 1, true);
    } else {
        setSelectedIndexInternal(selectedIndex_ + 1, true);
    }
    return true;
}

void TabView::setCursorSound(const std::string& filename) {
    cursorSound_ = filename;
}

const std::string& TabView::getCursorSound() const {
    return cursorSound_;
}

void TabView::setKeyHint(const KeyHint& leftHint, const KeyHint& rightHint) {
    KeyHintText parsedLeft = parseKeyHint(leftHint, "TabView left key hint");
    KeyHintText parsedRight = parseKeyHint(rightHint, "TabView right key hint");
    leftHint_ = std::move(parsedLeft);
    rightHint_ = std::move(parsedRight);
    updateHintVisibility();
}

sf::FloatRect TabView::getLocalBounds() const {
    return {{0.0f, 0.0f}, size_};
}

void TabView::update(float deltaTime) {
    selectionRect_->update(deltaTime);
    updateHintVisibility();
    FunctionalBase::update(deltaTime);
}

void TabView::onClick(const UiInputEventArguments& arguments) {
    if (suppressNextClick_) {
        suppressNextClick_ = false;
    } else if (isInteractionEnabled()) {
        const std::optional<sf::Vector2f> position =
            ludork::engine::ui_interaction::pointerPosition(arguments);
        if (position.has_value()) {
            selectPointerPosition(*position);
        }
    }
    FunctionalBase::onClick(arguments);
}

bool TabView::onMouseButtonDown(const UiInputEventArguments& arguments) {
    const bool callbackHandled = FunctionalBase::onMouseButtonDown(arguments);
    suppressNextClick_ = true;
    if (!isInteractionEnabled()) {
        return callbackHandled;
    }
    const std::optional<int> button =
        ludork::engine::ui_interaction::pointerButtonIndex(arguments);
    const std::optional<sf::Vector2f> position =
        ludork::engine::ui_interaction::pointerPosition(arguments);
    const int leftButton = static_cast<int>(sf::Mouse::Button::Left);
    if (!button.has_value() || *button != leftButton || !position.has_value()) {
        return callbackHandled;
    }
    return tabIndexAt(toLocalPosition(*position)).has_value() ||
           callbackHandled;
}

void TabView::onMouseMoved(const UiInputEventArguments& arguments) {
    selectMouseHover(arguments);
    FunctionalBase::onMouseMoved(arguments);
}

void TabView::refreshDisplayScale() {
    selectionRect_->refreshDisplayScale();
    for (const std::unique_ptr<PlainText>& label : labels_) {
        label->refreshDisplayScale();
    }
    leftHintBackground_->refreshDisplayScale();
    rightHintBackground_->refreshDisplayScale();
    leftHintGlyph_->refreshDisplayScale();
    rightHintGlyph_->refreshDisplayScale();
    layoutVisuals();
    ControlBase::refreshDisplayScale();
}

void TabView::releaseRuntimeCallbacks() noexcept {
    selectedIndexChangedCallback_ = {};
    ControlBase::releaseRuntimeCallbacks();
}

void TabView::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    _applyRenderStates(states);
    if (!getVisible()) {
        return;
    }
    for (const std::unique_ptr<PlainText>& label : labels_) {
        target.draw(*label, states);
    }
    target.draw(*selectionRect_, states);
    target.draw(*leftHintBackground_, states);
    target.draw(*rightHintBackground_, states);
    if (leftHintBackground_->getVisible()) {
        leftHintGlyph_->draw(target, states);
    }
    if (rightHintBackground_->getVisible()) {
        rightHintGlyph_->draw(target, states);
    }
}

void TabView::_refreshPresentationColour() {
    applyPresentationColour();
}

bool TabView::acceptsTouchCapture() const {
    return true;
}

void TabView::onTouchCaptureBegan(const sf::Vector2f&) {
    suppressNextClick_ = false;
}

sf::Vector2f TabView::normalizedSize(const sf::Vector2f& size) {
    return ludork::engine::tab_view_impl::normalizedSize(size);
}

int TabView::clampedIndex(int index, std::size_t count) {
    return ludork::engine::tab_view_impl::clampedIndex(index, count);
}

TabView::KeyHintText TabView::parseKeyHint(const KeyHint& values,
                                           const std::string& source) {
    KeyHintText result;
    if (values.Keyboard) {
        result.keyboard = ludork::engine::tab_view_impl::keyboardKeyText(
            *values.Keyboard, source + ".Keyboard");
    }
    if (values.Joystick) {
        if (!JoystickButton::isValid(*values.Joystick)) {
            throw std::invalid_argument(
                source +
                ".Joystick must match a registered JoystickButton value");
        }
        result.joystick = values.Joystick;
    }
    return result;
}

bool TabView::anyJoystickConnected() {
    return ludork::engine::ui_interaction::anyJoystickConnected();
}

bool TabView::keyboardHintsAvailableWithoutJoystick() {
    return ludork::engine::tab_view_impl::
        keyboardHintsAvailableWithoutJoystick();
}

bool TabView::setSelectedIndexInternal(int index, bool playSound) {
    const int selectedIndex = clampedIndex(index, items_.size());
    if (selectedIndex_ == selectedIndex) {
        return false;
    }
    selectedIndex_ = selectedIndex;
    updateSelectionVisual();
    updateHintVisibility();
    if (playSound) {
        playUiSound(cursorSound_);
    }
    if (selectedIndexChangedCallback_) {
        selectedIndexChangedCallback_(selectedIndex_);
    }
    return true;
}

bool TabView::selectPointerPosition(const sf::Vector2f& screenPosition) {
    const std::optional<int> index =
        tabIndexAt(toLocalPosition(screenPosition));
    if (!index.has_value()) {
        return false;
    }
    setSelectedIndexInternal(*index, true);
    return true;
}

void TabView::selectMouseHover(const UiInputEventArguments& arguments) {
    if (!isInteractionEnabled() || hasTouchCapture()) {
        return;
    }
    const std::optional<sf::Vector2f> position =
        ludork::engine::ui_interaction::pointerPosition(arguments);
    if (position.has_value()) {
        selectPointerPosition(*position);
    }
}

std::optional<int> TabView::tabIndexAt(
    const sf::Vector2f& localPosition) const {
    return ludork::engine::tab_view_impl::tabIndexAt(size_, localPosition,
                                                     items_.size(), HintSize);
}

sf::Vector2f TabView::toLocalPosition(
    const sf::Vector2f& screenPosition) const {
    return screenRenderTransform().getInverse().transformPoint(screenPosition) /
           engineState().getScale();
}

void TabView::rebuildVisuals() {
    selectionRect_ =
        std::make_unique<Rect>(sf::IntRect({0, 0}, {1, 1}), windowSkin_,
                               Rect::SelectionRectOpacityCurveKey);
    leftHintBackground_ =
        std::make_unique<SolidRect>(sf::Vector2f(HintSize, HintSize));
    rightHintBackground_ =
        std::make_unique<SolidRect>(sf::Vector2f(HintSize, HintSize));
    rebuildLabels();
    rebuildHintVisuals();
    layoutVisuals();
    applyPresentationColour();
}

void TabView::rebuildLabels() {
    labels_.clear();
    labels_.reserve(items_.size());
    for (const std::string& item : items_) {
        labels_.push_back(std::make_unique<PlainText>(textConfig_, item));
    }
}

void TabView::rebuildHintVisuals() {
    const std::shared_ptr<PlainTextConfig> config = hintTextConfig(textConfig_);
    leftHintGlyph_ =
        std::make_unique<ludork::engine::ui_interaction::GamepadGlyphImpl>(
            config);
    rightHintGlyph_ =
        std::make_unique<ludork::engine::ui_interaction::GamepadGlyphImpl>(
            config);
}

void TabView::layoutVisuals() {
    const float contentWidth =
        ludork::engine::tab_view_impl::contentWidth(size_.x, HintSize);
    const float slotWidth = ludork::engine::tab_view_impl::slotWidth(
        size_.x, HintSize, items_.size());
    selectionRect_->resize({slotWidth, size_.y});
    for (std::size_t index = 0; index < labels_.size(); ++index) {
        layoutLabel(*labels_[index], static_cast<int>(index));
    }
    layoutHint(*leftHintGlyph_, *leftHintBackground_, true);
    layoutHint(*rightHintGlyph_, *rightHintBackground_, false);
    setTouchHitBounds(sf::FloatRect({HintSize, 0.0f}, {contentWidth, size_.y}));
    updateSelectionVisual();
    updateHintVisibility();
}

void TabView::layoutLabel(PlainText& label, int index) const {
    const sf::FloatRect bounds = label.getLocalBounds();
    label.setOrigin({0.0f, 0.0f});
    label.setPosition(ludork::engine::tab_view_impl::labelPosition(
        bounds, size_, index, items_.size(), HintSize));
}

void TabView::layoutHint(
    ludork::engine::ui_interaction::GamepadGlyphImpl& glyph,
    SolidRect& background, bool left) const {
    const sf::Vector2f position =
        ludork::engine::tab_view_impl::hintPosition(size_, left, HintSize);
    background.setPosition(position);
    const float inset = (HintSize - HintContentSize) * 0.5f;
    glyph.layout(position + sf::Vector2f(inset, inset),
                 {HintContentSize, HintContentSize});
}

void TabView::updateSelectionVisual() {
    selectionRect_->setPosition(
        ludork::engine::tab_view_impl::selectionPosition(
            size_.x, items_.size(), selectedIndex_, HintSize));
}

void TabView::updateHintVisibility() {
    const bool joystick = anyJoystickConnected();
    const bool keyboard = !joystick && keyboardHintsAvailableWithoutJoystick();
    const auto update =
        [this, joystick, keyboard](
            const KeyHintText& hint,
            ludork::engine::ui_interaction::GamepadGlyphImpl& glyph,
            SolidRect& background, bool left, bool selectable) {
            const bool visible =
                selectable &&
                (joystick ? hint.joystick.has_value()
                          : keyboard && hint.keyboard.has_value());
            background.setVisible(visible);
            if (!visible) {
                return;
            }
            if (joystick) {
                glyph.setButton(*hint.joystick);
            } else {
                glyph.setText(*hint.keyboard);
            }
            layoutHint(glyph, background, left);
        };
    update(leftHint_, *leftHintGlyph_, *leftHintBackground_, true,
           selectedIndex_ > 0);
    update(rightHint_, *rightHintGlyph_, *rightHintBackground_, false,
           selectedIndex_ + 1 < static_cast<int>(items_.size()));
}

void TabView::applyPresentationColour() {
    selectionRect_->setPresentationColour(this, presentationColour());
    for (const std::unique_ptr<PlainText>& label : labels_) {
        label->setPresentationColour(this, presentationColour());
    }
    leftHintBackground_->setPresentationColour(this, presentationColour());
    rightHintBackground_->setPresentationColour(this, presentationColour());
    leftHintGlyph_->setColour(sf::Color(0, 0, 0, presentationColour().a));
    rightHintGlyph_->setColour(sf::Color(0, 0, 0, presentationColour().a));
}
