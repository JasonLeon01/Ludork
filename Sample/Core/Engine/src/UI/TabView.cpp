#include <UI/TabView.hpp>

#include <Input/InputService.hpp>
#include <Runtime/EngineState.hpp>
#include <UI/Rect.hpp>
#include <UI/SolidRect.hpp>
#include <UI/UiAudioService.hpp>

#include <LudorkCoreBinding/RegistryReference.hpp>
#include <LudorkPlatform.hpp>

#include <SFML/Window/Joystick.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#if defined(SFML_SYSTEM_HARMONY) && defined(SFML_HARMONY_MOBILE)
#include <deviceinfo.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

constexpr float HintSize = 16.0f;
constexpr float HintContentSize = 14.0f;
constexpr unsigned int HintCharacterSize = 8;

std::optional<double> numericValue(const RuntimeValue& value) {
    if (const double* number = value.getIf<double>()) {
        return *number;
    }
    if (const std::int64_t* number = value.getIf<std::int64_t>()) {
        return static_cast<double>(*number);
    }
    return std::nullopt;
}

std::string toUtf8String(const sf::String& value) {
    const sf::U8String bytes = value.toUtf8();
    return {bytes.begin(), bytes.end()};
}

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
    const bool handleLeft = input->isAnyJoystickButtonTriggered(
        static_cast<unsigned int>(JoystickButton::LB), false);
    const bool keyboardRight = input->isKeyTriggered(
        sf::Keyboard::Key::E, false, false, false, false, false);
    const bool handleRight = input->isAnyJoystickButtonTriggered(
        static_cast<unsigned int>(JoystickButton::RB), false);
    if (!keyboardLeft && !handleLeft && !keyboardRight && !handleRight) {
        return false;
    }

    if (keyboardLeft) {
        input->isKeyTriggered(sf::Keyboard::Key::Q, false, false, false, false,
                              true);
    }
    if (handleLeft) {
        input->isAnyJoystickButtonTriggered(
            static_cast<unsigned int>(JoystickButton::LB), true);
    }
    if (keyboardRight) {
        input->isKeyTriggered(sf::Keyboard::Key::E, false, false, false, false,
                              true);
    }
    if (handleRight) {
        input->isAnyJoystickButtonTriggered(
            static_cast<unsigned int>(JoystickButton::RB), true);
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
    const std::optional<int> button = pointerButton(arguments);
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

bool TabView::acceptsTouchCapture() const {
    return true;
}

sf::Vector2f TabView::normalizedSize(const sf::Vector2f& size) {
    return {
        std::isfinite(size.x) ? std::max(0.0f, size.x) : 0.0f,
        std::isfinite(size.y) ? std::max(0.0f, size.y) : 0.0f,
    };
}

int TabView::clampedIndex(int index, std::size_t count) {
    return std::clamp(index, 0, static_cast<int>(count) - 1);
}

TabView::KeyHint TabView::parseKeyHint(const RuntimeValue::Map& values,
                                       const std::string& source) {
    KeyHint result;
    for (const auto& [name, value] : values) {
        if (name == "Keyboard") {
            result.keyboard = keyboardKeyText(value, source + ".Keyboard");
        } else if (name == "Joystick") {
            result.handle = handleKeyText(value, source + ".Handle");
        } else {
            throw std::invalid_argument(source + " has unknown key " + name);
        }
    }
    return result;
}

std::string TabView::keyboardKeyText(const RuntimeValue& value,
                                     const std::string& source) {
    const std::int64_t* code = value.getIf<std::int64_t>();
    if (code == nullptr || *code < 0 ||
        static_cast<std::uint64_t>(*code) >= sf::Keyboard::KeyCount) {
        throw std::invalid_argument(
            source + " must be a valid non-Unknown sf.Keyboard.Key");
    }
    const sf::Keyboard::Key key = static_cast<sf::Keyboard::Key>(*code);
    const sf::Keyboard::Scancode scan = sf::Keyboard::delocalize(key);
    std::string result = toUtf8String(sf::Keyboard::getDescription(scan));
    if (result.empty()) {
        result = std::to_string(*code);
    }
    return result;
}

std::string TabView::handleKeyText(const RuntimeValue& value,
                                   const std::string& source) {
    const RuntimeIdentityPtr* identity = value.getIf<RuntimeIdentityPtr>();
    const auto* opaque =
        identity == nullptr || *identity == nullptr
            ? nullptr
            : dynamic_cast<
                  const ludork_core::LuaOpaqueIdentity<RuntimeIdentity>*>(
                  identity->get());
    if (opaque == nullptr) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    sol::state_view lua(opaque->value().state());
    const sol::object luaValue =
        ludork_core::readLuaRegistryReference(lua, opaque->value());
    if (!luaValue.is<sol::table>()) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    const sol::object engineValue =
        lua.globals().raw_get<sol::object>("Engine");
    if (!engineValue.is<sol::table>()) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    const sol::object inputValue =
        engineValue.as<sol::table>().raw_get<sol::object>("Input");
    if (!inputValue.is<sol::table>()) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    const sol::object reverseValue =
        inputValue.as<sol::table>().raw_get<sol::object>("JoyStickButtonName");
    if (!reverseValue.is<sol::table>()) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    const sol::object nameValue =
        reverseValue.as<sol::table>().raw_get<sol::object>(luaValue);
    if (!nameValue.is<std::string>()) {
        throw std::invalid_argument(source +
                                    " must be an Engine.JoystickButton value");
    }
    const std::string name = nameValue.as<std::string>();
    const auto iterator = inputJoystickButtons.find(name);
    if (iterator == inputJoystickButtons.end()) {
        throw std::invalid_argument(
            source + " must match a registered Engine.JoystickButton value");
    }
    return iterator->second.name;
}

std::optional<sf::Vector2f> TabView::pointerPosition(
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

std::optional<int> TabView::pointerButton(const RuntimeValue::Map& arguments) {
    const auto iterator = arguments.find("button");
    if (iterator == arguments.end()) {
        return std::nullopt;
    }
    const std::optional<double> value = numericValue(iterator->second);
    return value.has_value() ? std::optional<int>(static_cast<int>(*value))
                             : std::nullopt;
}

bool TabView::anyJoystickConnected() {
    for (unsigned int joystickId = 0; joystickId < sf::Joystick::Count;
         ++joystickId) {
        if (sf::Joystick::isConnected(joystickId)) {
            return true;
        }
    }
    return false;
}

bool TabView::keyboardHintsAvailableWithoutJoystick() {
#if !defined(LUDORK_MOBILE)
    return true;
#elif defined(SFML_SYSTEM_HARMONY) && defined(SFML_HARMONY_MOBILE)
    const char* deviceType = OH_GetDeviceType();
    return deviceType != nullptr && std::string_view(deviceType) == "tablet";
#else
    return false;
#endif
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
    const float right = size_.x - HintSize;
    if (localPosition.x < HintSize || localPosition.x >= right ||
        localPosition.y < 0.0f || localPosition.y >= size_.y ||
        right <= HintSize) {
        return std::nullopt;
    }
    const float slotWidth =
        (size_.x - HintSize * 2.0f) / static_cast<float>(items_.size());
    if (slotWidth <= 0.0f) {
        return std::nullopt;
    }
    const int index =
        static_cast<int>((localPosition.x - HintSize) / slotWidth);
    return std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
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
    const float contentWidth = std::max(0.0f, size_.x - HintSize * 2.0f);
    const float slotWidth = contentWidth / static_cast<float>(items_.size());
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
    const float contentWidth = std::max(0.0f, size_.x - HintSize * 2.0f);
    const float slotWidth = contentWidth / static_cast<float>(items_.size());
    const sf::FloatRect bounds = label.getLocalBounds();
    label.setOrigin({0.0f, 0.0f});
    label.setPosition(
        {HintSize + slotWidth * (static_cast<float>(index) + 0.5f) -
             bounds.position.x - bounds.size.x * 0.5f,
         size_.y * 0.5f - bounds.position.y - bounds.size.y * 0.5f});
}

void TabView::layoutHint(PlainText& text, SolidRect& background,
                         bool left) const {
    const float x = left ? 0.0f : size_.x - HintSize;
    const float y = (size_.y - HintSize) * 0.5f;
    background.setPosition({x, y});
    const sf::FloatRect bounds = text.getLocalBounds();
    float fitScale = 1.0f;
    if (bounds.size.x > 0.0f) {
        fitScale = std::min(fitScale, HintContentSize / bounds.size.x);
    }
    if (bounds.size.y > 0.0f) {
        fitScale = std::min(fitScale, HintContentSize / bounds.size.y);
    }
    text.setOrigin({0.0f, 0.0f});
    text.setScale({fitScale, fitScale});
    text.setPosition(
        {x + HintSize * 0.5f -
             (bounds.position.x + bounds.size.x * 0.5f) * fitScale,
         y + HintSize * 0.5f -
             (bounds.position.y + bounds.size.y * 0.5f) * fitScale});
}

void TabView::updateSelectionVisual() {
    const float contentWidth = std::max(0.0f, size_.x - HintSize * 2.0f);
    const float slotWidth = contentWidth / static_cast<float>(items_.size());
    selectionRect_->setPosition(
        {HintSize + slotWidth * static_cast<float>(selectedIndex_), 0.0f});
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
