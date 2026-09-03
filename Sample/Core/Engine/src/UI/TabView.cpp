#include <UI/TabView.hpp>

#include "Interaction/InputArguments.hpp"
#include "TabView/KeyHintRuntime.hpp"
#include "TabView/NavigationRuntime.hpp"
#include "TabView/VisualLayout.hpp"

#include <Input/InputService.hpp>
#include <Runtime/EngineState.hpp>
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

using ludork::engine::ui_interaction::pointerButtonIndex;
using ludork::engine::ui_interaction::pointerPosition;

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

void TabView::setItems(const std::vector<std::string>& items,
                       std::optional<std::function<void(int)>> callback) {
    if (items.size() != items_.size()) {
        throw std::invalid_argument(
            "TabView item count cannot change after construction");
    }
    items_ = items;
    for (std::size_t index = 0; index < items_.size(); ++index) {
        labels_[index]->setString(items_[index]);
        layoutLabel(*labels_[index], static_cast<int>(index));
    }
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
    InputService* input = dynamic_cast<InputService*>(inputProvider());
    if (input == nullptr) {
        return false;
    }

    const bool keyboardLeft = input->isKeyTriggered(sf::Keyboard::Key::Q, false,
                                                    false, false, false, false);
    const InputNamedValue leftButton = JoystickButton::getLB();
    const bool handleLeft = input->isAnyJoystickButtonTriggered(
        static_cast<unsigned int>(leftButton.value), false);
    const bool keyboardRight = input->isKeyTriggered(
        sf::Keyboard::Key::E, false, false, false, false, false);
    const InputNamedValue rightButton = JoystickButton::getRB();
    const bool handleRight = input->isAnyJoystickButtonTriggered(
        static_cast<unsigned int>(rightButton.value), false);
    if (!keyboardLeft && !handleLeft && !keyboardRight && !handleRight) {
        return false;
    }

    if (keyboardLeft) {
        input->isKeyTriggered(sf::Keyboard::Key::Q, false, false, false, false,
                              true);
    }
    if (handleLeft) {
        input->isAnyJoystickButtonTriggered(
            static_cast<unsigned int>(leftButton.value), true);
    }
    if (keyboardRight) {
        input->isKeyTriggered(sf::Keyboard::Key::E, false, false, false, false,
                              true);
    }
    if (handleRight) {
        input->isAnyJoystickButtonTriggered(
            static_cast<unsigned int>(rightButton.value), true);
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

void TabView::setKeyHint(const RuntimeValue::Map& leftHint,
                         const RuntimeValue::Map& rightHint) {
    KeyHint parsedLeft = parseKeyHint(leftHint, "TabView left key hint");
    KeyHint parsedRight = parseKeyHint(rightHint, "TabView right key hint");
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
    suppressNextClick_ = false;
}

void TabView::onClick(const RuntimeValue::Map& arguments) {
    if (suppressNextClick_) {
        suppressNextClick_ = false;
    } else if (isInteractionEnabled()) {
        const std::optional<sf::Vector2f> position = pointerPosition(arguments);
        if (position.has_value()) {
            selectPointerPosition(*position);
        }
    }
    FunctionalBase::onClick(arguments);
}

bool TabView::onMouseButtonDown(const RuntimeValue::Map& arguments) {
    const bool callbackHandled = FunctionalBase::onMouseButtonDown(arguments);
    suppressNextClick_ = true;
    if (!isInteractionEnabled()) {
        return callbackHandled;
    }
    const std::optional<int> button = pointerButtonIndex(arguments);
    const std::optional<sf::Vector2f> position = pointerPosition(arguments);
    const int leftButton = static_cast<int>(sf::Mouse::Button::Left);
    if (!button.has_value() || *button != leftButton || !position.has_value()) {
        return callbackHandled;
    }
    return tabIndexAt(toLocalPosition(*position)).has_value() ||
           callbackHandled;
}

void TabView::onMouseMoved(const RuntimeValue::Map& arguments) {
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
    leftHintText_->refreshDisplayScale();
    rightHintText_->refreshDisplayScale();
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
    target.draw(*leftHintText_, states);
    target.draw(*rightHintText_, states);
}

void TabView::_refreshPresentationColour() {
    applyPresentationColour();
}

bool TabView::acceptsTouchCapture() const {
    return true;
}

sf::Vector2f TabView::normalizedSize(const sf::Vector2f& size) {
    return ludork::engine::tab_view_impl::normalizedSize(size);
}

int TabView::clampedIndex(int index, std::size_t count) {
    return ludork::engine::tab_view_impl::clampedIndex(index, count);
}

TabView::KeyHint TabView::parseKeyHint(const RuntimeValue::Map& values,
                                       const std::string& source) {
    const auto result =
        ludork::engine::tab_view_impl::parseKeyHint(values, source);
    return {result.keyboard, result.handle};
}

bool TabView::anyJoystickConnected() {
    return ludork::engine::tab_view_impl::anyJoystickConnected();
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

void TabView::selectMouseHover(const RuntimeValue::Map& arguments) {
    if (!isInteractionEnabled() || hasTouchCapture()) {
        return;
    }
    const std::optional<sf::Vector2f> position = pointerPosition(arguments);
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
           Scale;
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
    std::shared_ptr<PlainTextConfig> config = hintTextConfig(textConfig_);
    leftHintText_ = std::make_unique<PlainText>(config, "");
    rightHintText_ = std::make_unique<PlainText>(std::move(config), "");
    leftHintText_->setColour(sf::Color::Black);
    rightHintText_->setColour(sf::Color::Black);
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
    layoutHint(*leftHintText_, *leftHintBackground_, true);
    layoutHint(*rightHintText_, *rightHintBackground_, false);
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

void TabView::layoutHint(PlainText& text, SolidRect& background,
                         bool left) const {
    const sf::FloatRect bounds = text.getLocalBounds();
    const ludork::engine::tab_view_impl::HintLayout layout =
        ludork::engine::tab_view_impl::hintLayout(bounds, size_, left, HintSize,
                                                  HintContentSize);
    background.setPosition(layout.backgroundPosition);
    text.setOrigin({0.0f, 0.0f});
    text.setScale({layout.textScale, layout.textScale});
    text.setPosition(layout.textPosition);
}

void TabView::updateSelectionVisual() {
    selectionRect_->setPosition(
        ludork::engine::tab_view_impl::selectionPosition(
            size_.x, items_.size(), selectedIndex_, HintSize));
}

void TabView::updateHintVisibility() {
    const std::optional<std::string>& left = visibleHint(leftHint_);
    const std::optional<std::string>& right = visibleHint(rightHint_);
    const bool showLeft = selectedIndex_ > 0 && left.has_value();
    const bool showRight =
        selectedIndex_ + 1 < static_cast<int>(items_.size()) &&
        right.has_value();
    leftHintBackground_->setVisible(showLeft);
    leftHintText_->setVisible(showLeft);
    rightHintBackground_->setVisible(showRight);
    rightHintText_->setVisible(showRight);
    if (showLeft && leftHintText_->getString() != *left) {
        leftHintText_->setString(*left);
        layoutHint(*leftHintText_, *leftHintBackground_, true);
    }
    if (showRight && rightHintText_->getString() != *right) {
        rightHintText_->setString(*right);
        layoutHint(*rightHintText_, *rightHintBackground_, false);
    }
}

void TabView::applyPresentationColour() {
    selectionRect_->setPresentationColour(this, presentationColour());
    for (const std::unique_ptr<PlainText>& label : labels_) {
        label->setPresentationColour(this, presentationColour());
    }
    leftHintBackground_->setPresentationColour(this, presentationColour());
    rightHintBackground_->setPresentationColour(this, presentationColour());
    leftHintText_->setPresentationColour(this, presentationColour());
    rightHintText_->setPresentationColour(this, presentationColour());
}

const std::optional<std::string>& TabView::visibleHint(
    const KeyHint& hint) const {
    if (anyJoystickConnected()) {
        return hint.handle;
    }
    if (keyboardHintsAvailableWithoutJoystick()) {
        return hint.keyboard;
    }
    static const std::optional<std::string> hidden;
    return hidden;
}
