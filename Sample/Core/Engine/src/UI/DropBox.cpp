#include <UI/DropBox.hpp>

#include <Input/InputService.hpp>
#include <Runtime/EngineState.hpp>
#include <UI/Rect.hpp>
#include <UI/UiAudioService.hpp>
#include <UI/Window.hpp>

#include <LudorkPlatform.hpp>

#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace {

constexpr float RepeatDelay = 0.4f;
constexpr float RepeatInterval = 0.1f;
constexpr float ItemHorizontalInset = 32.0f;
constexpr float CollapsedTextInset = 8.0f;
constexpr float ExpandedContentTop = 16.0f;

#if defined(LUDORK_MOBILE)
constexpr bool MobilePlatform = true;
#else
constexpr bool MobilePlatform = false;
#endif

std::optional<double> numericValue(const RuntimeValue& value) {
    if (const double* result = value.getIf<double>()) {
        return *result;
    }
    if (const std::int64_t* result = value.getIf<std::int64_t>()) {
        return static_cast<double>(*result);
    }
    return std::nullopt;
}

}  // namespace

DropBox::DropBox(const sf::Vector2f& collapsedSize, const sf::Image& windowSkin,
                 std::shared_ptr<PlainTextConfig> textConfig,
                 std::vector<std::string> items, int selectedIndex,
                 bool repeated)
    : collapsedSize_(normalizedSize(collapsedSize)),
      windowSkin_(windowSkin),
      textConfig_(std::move(textConfig)),
      items_(std::move(items)),
      repeated_(repeated) {
    if (textConfig_ == nullptr) {
        throw std::invalid_argument("DropBox text config must not be null");
    }
    selectedIndex_ = clampedIndex(selectedIndex);
    cursorIndex_ = selectedIndex_;
    setCanReceiveFocus(true);
}

DropBox::~DropBox() = default;

sf::Vector2f DropBox::getSize() const {
    return expanded_ ? sf::Vector2f(collapsedSize_.x, expandedHeight())
                     : collapsedSize_;
}

sf::Vector2f DropBox::getCollapsedSize() const {
    return collapsedSize_;
}

void DropBox::resize(const sf::Vector2f& size) {
    setCollapsedSize(size);
}

void DropBox::setCollapsedSize(const sf::Vector2f& size) {
    const sf::Vector2f normalized = normalizedSize(size);
    if (normalized == collapsedSize_) {
        return;
    }
    const sf::Vector2f previousSize = getSize();
    collapsedSize_ = normalized;
    markVisualsDirty();
    if (layoutChangedCallback_ && getSize() != previousSize) {
        layoutChangedCallback_();
    }
}

void DropBox::setWindowSkin(const sf::Image& windowSkin, bool repeated) {
    windowSkin_ = windowSkin;
    repeated_ = repeated;
    markVisualsDirty();
}

void DropBox::setTextConfig(std::shared_ptr<PlainTextConfig> textConfig) {
    if (textConfig == nullptr) {
        throw std::invalid_argument("DropBox text config must not be null");
    }
    textConfig_ = std::move(textConfig);
    markVisualsDirty();
}

std::vector<std::string> DropBox::getItems() const {
    return items_;
}

void DropBox::setItems(const std::vector<std::string>& items) {
    const sf::Vector2f previousSize = getSize();
    items_ = items;
    selectedIndex_ = clampedIndex(selectedIndex_);
    cursorIndex_ = selectedIndex_;
    markVisualsDirty();
    if (layoutChangedCallback_ && getSize() != previousSize) {
        layoutChangedCallback_();
    }
}

int DropBox::getSelectedIndex() const {
    return selectedIndex_;
}

void DropBox::setSelectedIndex(int index) {
    const int selectedIndex = clampedIndex(index);
    if (selectedIndex_ == selectedIndex) {
        return;
    }
    selectedIndex_ = selectedIndex;
    cursorIndex_ = selectedIndex_;
    markVisualsDirty();
    if (selectedIndexChangedCallback_) {
        selectedIndexChangedCallback_(selectedIndex_);
    }
}

std::string DropBox::getSelectedItem() const {
    return items_.empty() ? std::string() : items_[selectedIndex_];
}

bool DropBox::isExpanded() const {
    return expanded_;
}

void DropBox::setExpanded(bool expanded) {
    setExpandedState(expanded);
}

void DropBox::open() {
    if (expanded_) {
        return;
    }
    playUiSound(openSound_);
    setExpandedState(true);
}

void DropBox::cancel() {
    if (!expanded_) {
        return;
    }
    playUiSound(cancelSound_);
    setExpandedState(false);
}

void DropBox::setOnSelectedIndexChanged(std::function<void(int)> callback) {
    selectedIndexChangedCallback_ = std::move(callback);
}

void DropBox::setOnSelectionConfirmed(std::function<void(int)> callback) {
    selectionConfirmedCallback_ = std::move(callback);
}

void DropBox::setOnExpandedChanged(std::function<void(bool)> callback) {
    expandedChangedCallback_ = std::move(callback);
}

void DropBox::setOnLayoutChanged(std::function<void()> callback) {
    layoutChangedCallback_ = std::move(callback);
}

void DropBox::setOpenSound(const std::string& filename) {
    openSound_ = filename;
}

const std::string& DropBox::getOpenSound() const {
    return openSound_;
}

void DropBox::setCursorSound(const std::string& filename) {
    cursorSound_ = filename;
}

const std::string& DropBox::getCursorSound() const {
    return cursorSound_;
}

void DropBox::setSelectSound(const std::string& filename) {
    selectSound_ = filename;
}

const std::string& DropBox::getSelectSound() const {
    return selectSound_;
}

void DropBox::setCancelSound(const std::string& filename) {
    cancelSound_ = filename;
}

const std::string& DropBox::getCancelSound() const {
    return cancelSound_;
}

void DropBox::update(float deltaTime) {
    ensureVisuals();
    if (expanded_ && selectionRect_ != nullptr && !items_.empty()) {
        selectionRect_->update(deltaTime);
    }
    FunctionalBase::update(deltaTime);
    suppressNextClick_ = false;
}

void DropBox::onConfirm(const RuntimeValue::Map& arguments) {
    if (MobilePlatform && hasTouchCapture()) {
        const std::optional<sf::Vector2f> position = pointerPosition(arguments);
        if (position.has_value()) {
            handlePointerAction(*position, std::nullopt);
            FunctionalBase::onConfirm(arguments);
            return;
        }
    }
    if (expanded_) {
        confirmCurrentSelection();
    } else {
        open();
    }
    FunctionalBase::onConfirm(arguments);
}

void DropBox::onCancel(const RuntimeValue::Map& arguments) {
    cancel();
    FunctionalBase::onCancel(arguments);
}

void DropBox::onClick(const RuntimeValue::Map& arguments) {
    if (suppressNextClick_) {
        suppressNextClick_ = false;
    } else if (const std::optional<sf::Vector2f> position =
                   pointerPosition(arguments);
               position.has_value()) {
        handlePointerAction(*position, std::nullopt);
    }
    FunctionalBase::onClick(arguments);
}

bool DropBox::onMouseButtonDown(const RuntimeValue::Map& arguments) {
    const std::optional<sf::Vector2f> position = pointerPosition(arguments);
    const std::optional<int> button = pointerButton(arguments);
    if (position.has_value() && handlePointerAction(*position, button)) {
        suppressNextClick_ = true;
        return true;
    }
    const int leftButton = static_cast<int>(sf::Mouse::Button::Left);
    if (!button.has_value() || *button != leftButton) {
        suppressNextClick_ = true;
    }
    return FunctionalBase::onMouseButtonDown(arguments);
}

void DropBox::onMouseMoved(const RuntimeValue::Map& arguments) {
    const std::optional<sf::Vector2f> position = pointerPosition(arguments);
    if (expanded_ && position.has_value() &&
        !(MobilePlatform && hasTouchCapture())) {
        const std::optional<int> index =
            itemIndexAt(toLocalPosition(*position));
        if (index.has_value()) {
            requestKeyboardFocus();
            if (cursorIndex_ != *index) {
                cursorIndex_ = *index;
                updateSelectionVisual();
                playUiSound(cursorSound_);
            }
        }
    }
    FunctionalBase::onMouseMoved(arguments);
}

void DropBox::onMouseWheelScrolled(const RuntimeValue::Map& arguments) {
    const auto iterator = arguments.find("delta");
    if (expanded_ && iterator != arguments.end()) {
        const std::optional<double> delta = numericValue(iterator->second);
        const int offset = delta.has_value() && *delta > 0.0   ? -1
                           : delta.has_value() && *delta < 0.0 ? 1
                                                               : 0;
        if (offset != 0 && moveCursor(offset, false)) {
            requestKeyboardFocus();
            playUiSound(cursorSound_);
        }
    }
    FunctionalBase::onMouseWheelScrolled(arguments);
}

void DropBox::onKeyDown(const RuntimeValue::Map& arguments) {
    InputService& input = inputService();
    if (expanded_) {
        if (input.isActionTriggered(input.getCancelKeys(), false)) {
            input.isActionTriggered(input.getCancelKeys(), true);
            onCancel(arguments);
            return;
        }
        if (input.isActionTriggered(input.getConfirmKeys(), false)) {
            input.isActionTriggered(input.getConfirmKeys(), true);
            onConfirm(arguments);
            return;
        }
        if (!items_.empty() &&
            input.isActionTriggered(input.getUpKeys(), false, RepeatDelay,
                                    RepeatInterval)) {
            input.isActionTriggered(input.getUpKeys(), true, RepeatDelay,
                                    RepeatInterval);
            if (moveCursor(-1, true)) {
                playUiSound(cursorSound_);
            }
            return;
        }
        if (!items_.empty() &&
            input.isActionTriggered(input.getDownKeys(), false, RepeatDelay,
                                    RepeatInterval)) {
            input.isActionTriggered(input.getDownKeys(), true, RepeatDelay,
                                    RepeatInterval);
            if (moveCursor(1, true)) {
                playUiSound(cursorSound_);
            }
            return;
        }
    } else if (input.isActionTriggered(input.getConfirmKeys(), false)) {
        input.isActionTriggered(input.getConfirmKeys(), true);
        onConfirm(arguments);
        return;
    }
    FunctionalBase::onKeyDown(arguments);
}

void DropBox::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (!getVisible()) {
        return;
    }
    ensureVisuals();
    _applyRenderStates(states);
    if (!expanded_) {
        target.draw(*collapsedFrame_, states);
        target.draw(*collapsedText_, states);
        return;
    }
    target.draw(*expandedWindow_, states);
    if (!items_.empty()) {
        target.draw(*selectionRect_, states);
    }
    for (const std::unique_ptr<PlainText>& text : itemTexts_) {
        target.draw(*text, states);
    }
}

sf::Vector2f DropBox::normalizedSize(const sf::Vector2f& size) {
    return {
        std::isfinite(size.x) ? std::max(0.0f, size.x) : 0.0f,
        std::isfinite(size.y) ? std::max(0.0f, size.y) : 0.0f,
    };
}

sf::Vector2i DropBox::roundedSize(const sf::Vector2f& size) {
    return {
        std::max(0, static_cast<int>(std::lround(size.x))),
        std::max(0, static_cast<int>(std::lround(size.y))),
    };
}

std::optional<sf::Vector2f> DropBox::pointerPosition(
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

std::optional<int> DropBox::pointerButton(const RuntimeValue::Map& arguments) {
    const auto iterator = arguments.find("button");
    if (iterator == arguments.end()) {
        return std::nullopt;
    }
    const std::optional<double> value = numericValue(iterator->second);
    return value.has_value() ? std::optional<int>(static_cast<int>(*value))
                             : std::nullopt;
}

int DropBox::clampedIndex(int index) const {
    if (items_.empty()) {
        return 0;
    }
    return std::clamp(index, 0, static_cast<int>(items_.size()) - 1);
}

float DropBox::expandedHeight() const {
    return ExpandedBorderHeight +
           RowHeight *
               static_cast<float>(std::max<std::size_t>(items_.size(), 1U));
}

void DropBox::setExpandedState(bool expanded) {
    if (expanded_ == expanded) {
        return;
    }
    const sf::Vector2f previousSize = getSize();
    expanded_ = expanded;
    if (expanded_) {
        previousCanReceiveFocus_ = getCanReceiveFocus();
        focusabilityOverridden_ = true;
        setCanReceiveFocus(true);
        cursorIndex_ = selectedIndex_;
    } else if (focusabilityOverridden_) {
        setCanReceiveFocus(previousCanReceiveFocus_);
        focusabilityOverridden_ = false;
    }
    updateSelectionVisual();
    if (layoutChangedCallback_ && getSize() != previousSize) {
        layoutChangedCallback_();
    }
    if (!expanded_) {
        restoreParentFocus();
    }
    if (expandedChangedCallback_) {
        expandedChangedCallback_(expanded_);
    }
    if (expanded_) {
        requestKeyboardFocus();
    }
}

void DropBox::confirmCurrentSelection() {
    if (!items_.empty()) {
        setSelectedIndex(cursorIndex_);
        if (selectionConfirmedCallback_) {
            selectionConfirmedCallback_(selectedIndex_);
        }
    }
    playUiSound(selectSound_);
    setExpandedState(false);
}

bool DropBox::moveCursor(int offset, bool wrap) {
    if (items_.empty() || offset == 0) {
        return false;
    }
    const int count = static_cast<int>(items_.size());
    int index = cursorIndex_ + offset;
    if (wrap) {
        index %= count;
        if (index < 0) {
            index += count;
        }
    } else {
        index = std::clamp(index, 0, count - 1);
    }
    if (cursorIndex_ == index) {
        return false;
    }
    cursorIndex_ = index;
    updateSelectionVisual();
    return true;
}

bool DropBox::handlePointerAction(const sf::Vector2f& screenPosition,
                                  std::optional<int> button) {
    const int rightButton = static_cast<int>(sf::Mouse::Button::Right);
    if (expanded_ && button.has_value() && *button == rightButton) {
        cancel();
        return true;
    }
    const sf::Vector2f localPosition = toLocalPosition(screenPosition);
    const sf::Vector2f size = getSize();
    if (!sf::FloatRect({0.0f, 0.0f}, size).contains(localPosition)) {
        return false;
    }
    const int leftButton = static_cast<int>(sf::Mouse::Button::Left);
    if (button.has_value() && *button == rightButton) {
        return false;
    }
    if (button.has_value() && *button != leftButton) {
        return false;
    }
    if (!expanded_) {
        open();
        return true;
    }
    const std::optional<int> index = itemIndexAt(localPosition);
    if (index.has_value()) {
        if (MobilePlatform && hasTouchCapture() && cursorIndex_ != *index) {
            cursorIndex_ = *index;
            updateSelectionVisual();
            playUiSound(cursorSound_);
            return true;
        }
        cursorIndex_ = *index;
        updateSelectionVisual();
        confirmCurrentSelection();
    }
    return true;
}

std::optional<int> DropBox::itemIndexAt(
    const sf::Vector2f& localPosition) const {
    if (!expanded_) {
        return std::nullopt;
    }
    const float inset = std::min(ItemHorizontalInset, collapsedSize_.x * 0.5f);
    if (localPosition.x < inset || localPosition.x > collapsedSize_.x - inset) {
        return std::nullopt;
    }
    const float relativeY = localPosition.y - ExpandedContentTop;
    if (relativeY < 0.0f) {
        return std::nullopt;
    }
    const int index = static_cast<int>(relativeY / RowHeight);
    const int itemCount =
        static_cast<int>(std::max<std::size_t>(items_.size(), 1U));
    if (index < 0 || index >= itemCount) {
        return std::nullopt;
    }
    return index;
}

sf::Vector2f DropBox::toLocalPosition(
    const sf::Vector2f& screenPosition) const {
    return screenRenderTransform().getInverse().transformPoint(screenPosition) /
           Scale;
}

void DropBox::restoreParentFocus() {
    std::shared_ptr<ControlBase> parent = getParent();
    while (parent != nullptr) {
        FunctionalBase* functional =
            dynamic_cast<FunctionalBase*>(parent.get());
        if (functional != nullptr && functional->canReceiveFocus()) {
            functional->requestKeyboardFocus();
            return;
        }
        parent = parent->getParent();
    }
}

void DropBox::markVisualsDirty() {
    visualsDirty_ = true;
}

void DropBox::refreshDisplayScale() {
    markVisualsDirty();
    ControlBase::refreshDisplayScale();
}

void DropBox::ensureVisuals() const {
    if (visualsDirty_) {
        rebuildVisuals();
    }
}

void DropBox::rebuildVisuals() const {
    const sf::Vector2i collapsedPixels = roundedSize(collapsedSize_);
    const sf::Vector2i expandedPixels =
        roundedSize({collapsedSize_.x, expandedHeight()});
    collapsedFrame_ = std::make_unique<Rect>(
        sf::IntRect({0, 0}, collapsedPixels), windowSkin_);
    collapsedText_ =
        std::make_unique<PlainText>(textConfig_, getSelectedItem());
    expandedWindow_ = std::make_unique<Window>(
        sf::IntRect({0, 0}, expandedPixels), windowSkin_, repeated_);
    const int selectionWidth = std::max(0, collapsedPixels.x - 64);
    selectionRect_ =
        std::make_unique<Rect>(sf::IntRect({32, 16}, {selectionWidth, 32}),
                               windowSkin_, Rect::SelectionRectOpacityCurveKey);
    itemTexts_.clear();
    itemTexts_.reserve(items_.size());
    for (std::size_t index = 0; index < items_.size(); ++index) {
        std::unique_ptr<PlainText> text =
            std::make_unique<PlainText>(textConfig_, items_[index]);
        positionItemText(*text, static_cast<int>(index));
        itemTexts_.push_back(std::move(text));
    }
    positionCollapsedText();
    updateSelectionVisual();
    visualsDirty_ = false;
}

void DropBox::updateSelectionVisual() const {
    if (selectionRect_ == nullptr) {
        return;
    }
    selectionRect_->setVisible(expanded_ && !items_.empty());
    selectionRect_->setPosition(
        {ItemHorizontalInset,
         ExpandedContentTop + RowHeight * static_cast<float>(cursorIndex_)});
}

void DropBox::positionCollapsedText() const {
    if (collapsedText_ == nullptr) {
        return;
    }
    const sf::FloatRect bounds = collapsedText_->getLocalBounds();
    collapsedText_->setPosition(
        {CollapsedTextInset - bounds.position.x,
         (collapsedSize_.y - bounds.size.y) * 0.5f - bounds.position.y});
}

void DropBox::positionItemText(PlainText& text, int index) const {
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition(
        {(collapsedSize_.x - bounds.size.x) * 0.5f - bounds.position.x,
         ExpandedContentTop + RowHeight * static_cast<float>(index) +
             (RowHeight - bounds.size.y) * 0.5f - bounds.position.y});
}
