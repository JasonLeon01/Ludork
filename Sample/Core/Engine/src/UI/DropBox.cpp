#include <UI/DropBox.hpp>

#include "DropBox/PopupRuntime.hpp"
#include "DropBox/VisualRuntime.hpp"
#include "Interaction/InputArguments.hpp"

#include <Input/InputService.hpp>
#include <Runtime/EngineState.hpp>
#include <UI/Canvas.hpp>
#include <UI/Rect.hpp>
#include <UI/UiAudioService.hpp>
#include <UI/Window.hpp>
#include <Utils/Math.hpp>
#include <Utils/Render.hpp>

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
constexpr float WheelScrollResponse = 18.0f;
constexpr float WheelScrollEpsilon = 0.01f;
constexpr float GeometryEpsilon = 0.001f;

#if defined(LUDORK_MOBILE)
constexpr bool MobilePlatform = true;
#else
constexpr bool MobilePlatform = false;
#endif

using ludork::engine::ui_interaction::numericValue;
using ludork::engine::ui_interaction::pointerButtonIndex;
using ludork::engine::ui_interaction::pointerPosition;

bool nearlyEqual(float left, float right) {
    return std::abs(left - right) <= GeometryEpsilon;
}

sf::FloatRect canvasContentScreenBounds(const Canvas& canvas) {
    sf::Transform transform = canvas.screenRenderTransform();
    const sf::Vector2f scrollOffset =
        canvas.getDefaultView().getCenter() - canvas.getView().getCenter();
    transform.translate(-scrollOffset * Scale);
    const sf::IntRect contentRect = canvas.getContentRect();
    const sf::FloatRect scaledContent(
        sf::Vector2f(contentRect.position) * Scale,
        sf::Vector2f(std::max(0, contentRect.size.x),
                     std::max(0, contentRect.size.y)) *
            Scale);
    return transform.transformRect(scaledContent);
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
    return collapsedSize_;
}

sf::FloatRect DropBox::getLocalBounds() const {
    if (!expanded_) {
        return {{0.0f, 0.0f}, collapsedSize_};
    }
    syncPopupGeometry(false);
    const float minY = std::min(0.0f, popupGeometry_.positionY);
    const float maxY = std::max(
        collapsedSize_.y, popupGeometry_.positionY + popupGeometry_.height);
    return {{0.0f, minY}, {collapsedSize_.x, maxY - minY}};
}

void DropBox::resize(const sf::Vector2f& size) {
    const sf::Vector2f normalized = normalizedSize(size);
    if (normalized == collapsedSize_) {
        return;
    }
    collapsedSize_ = normalized;
    popupGeometryInitialized_ = false;
    markVisualsDirty();
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
    items_ = items;
    selectedIndex_ = clampedIndex(selectedIndex_);
    cursorIndex_ = selectedIndex_;
    scrollTargetOffset_.reset();
    popupGeometryInitialized_ = false;
    markVisualsDirty();
    if (expanded_) {
        syncPopupGeometry(true);
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
    if (expanded_) {
        syncPopupGeometry(false);
        ensureCursorVisible();
    }
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
    if (expanded_) {
        syncPopupGeometry(false);
        updateWheelScroll(deltaTime);
    }
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
    const std::optional<int> button = pointerButtonIndex(arguments);
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
    if (expanded_ && position.has_value() && MobilePlatform &&
        hasTouchCapture()) {
        if (inputService().isTouchDragged()) {
            scrollTargetOffset_.reset();
            const float localDelta = toLocalPosition(*position).y -
                                     toLocalPosition(touchStartPosition_).y;
            setScrollOffset(touchStartScrollOffset_ - localDelta);
        }
    } else if (expanded_ && position.has_value()) {
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
        const std::optional<sf::Mouse::Wheel> wheel =
            inputService().getMouseScrolledWheel();
        if (delta.has_value() && *delta != 0.0 &&
            (!wheel.has_value() || *wheel == sf::Mouse::Wheel::Vertical)) {
            syncPopupGeometry(false);
            if (inputService().isMouseWheelPrecise()) {
                scrollTargetOffset_.reset();
                setScrollOffset(scrollOffset_ - static_cast<float>(*delta) /
                                                    std::max(Scale, 0.000001f));
            } else {
                const float target =
                    scrollTargetOffset_.value_or(scrollOffset_) -
                    static_cast<float>(*delta) * RowHeight;
                setScrollTargetOffset(target);
            }
            requestKeyboardFocus();
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
    target.draw(*collapsedFrame_, states);
    target.draw(*collapsedText_, states);
    if (expanded_ && !hasCanvasAncestor()) {
        _drawOverlay(target, states);
    }
}

void DropBox::onTouchCaptureBegan(const sf::Vector2f& position) {
    touchStartPosition_ = position;
    touchStartScrollOffset_ = scrollOffset_;
    scrollTargetOffset_.reset();
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

DropBox::PopupGeometry DropBox::calculatePopupGeometry() const {
    const sf::FloatRect screenBounds({0.0f, 0.0f},
                                     toVector2f(GameSize) * Scale);
    sf::FloatRect constraint = screenBounds;
    std::shared_ptr<ControlBase> parent = getParent();
    std::shared_ptr<Canvas> host;
    while (parent != nullptr) {
        if (const std::shared_ptr<Canvas> canvas =
                std::dynamic_pointer_cast<Canvas>(parent)) {
            host = canvas;
        }
        parent = parent->getParent();
    }
    if (host != nullptr) {
        constraint = ludork::engine::drop_box_impl::intersectRects(
            constraint, canvasContentScreenBounds(*host));
    }

    PopupGeometry result;
    if (constraint.size.x <= 0.0f || constraint.size.y <= 0.0f) {
        return result;
    }
    const sf::FloatRect collapsedScreenBounds =
        screenRenderTransform().transformRect(
            {{0.0f, 0.0f}, collapsedSize_ * Scale});
    const sf::FloatRect visibleAnchor =
        ludork::engine::drop_box_impl::intersectRects(collapsedScreenBounds,
                                                      constraint);
    if (visibleAnchor.size.x <= 0.0f || visibleAnchor.size.y <= 0.0f) {
        return result;
    }

    const sf::FloatRect localConstraint =
        screenRenderTransform().getInverse().transformRect(constraint);
    const float naturalHeight = expandedHeight();
    const auto geometry = ludork::engine::drop_box_impl::calculatePopupGeometry(
        collapsedSize_, localConstraint, naturalHeight, items_.size(), Scale,
        ExpandedBorderHeight, RowHeight);
    result.positionY = geometry.positionY;
    result.height = geometry.height;
    result.contentHeight = geometry.contentHeight;
    result.maxScrollOffset = geometry.maxScrollOffset;
    return result;
}

void DropBox::syncPopupGeometry(bool ensureCursor) const {
    const PopupGeometry geometry = calculatePopupGeometry();
    const bool changed =
        !popupGeometryInitialized_ ||
        !nearlyEqual(popupGeometry_.positionY, geometry.positionY) ||
        !nearlyEqual(popupGeometry_.height, geometry.height) ||
        !nearlyEqual(popupGeometry_.contentHeight, geometry.contentHeight) ||
        !nearlyEqual(popupGeometry_.maxScrollOffset, geometry.maxScrollOffset);
    popupGeometry_ = geometry;
    popupGeometryInitialized_ = true;
    clampScrollOffsets();
    if (changed || ensureCursor) {
        ensureCursorVisible();
    }
}

void DropBox::clampScrollOffsets() const {
    scrollOffset_ = ludork::engine::drop_box_impl::clampScrollOffset(
        scrollOffset_, popupGeometry_.maxScrollOffset);
    if (scrollTargetOffset_.has_value()) {
        *scrollTargetOffset_ = ludork::engine::drop_box_impl::clampScrollOffset(
            *scrollTargetOffset_, popupGeometry_.maxScrollOffset);
    }
}

void DropBox::setScrollOffset(float offset) const {
    scrollOffset_ = ludork::engine::drop_box_impl::clampScrollOffset(
        offset, popupGeometry_.maxScrollOffset);
}

void DropBox::setScrollTargetOffset(float offset) const {
    scrollTargetOffset_ = ludork::engine::drop_box_impl::clampScrollOffset(
        offset, popupGeometry_.maxScrollOffset);
}

void DropBox::updateWheelScroll(float deltaTime) const {
    if (!scrollTargetOffset_.has_value()) {
        return;
    }
    const float distance = *scrollTargetOffset_ - scrollOffset_;
    if (std::abs(distance) <= WheelScrollEpsilon) {
        setScrollOffset(*scrollTargetOffset_);
        scrollTargetOffset_.reset();
        return;
    }
    setScrollOffset(ludork::engine::drop_box_impl::advanceScrollOffset(
        scrollOffset_, *scrollTargetOffset_, deltaTime, WheelScrollResponse));
}

void DropBox::ensureCursorVisible() const {
    scrollTargetOffset_.reset();
    if (items_.empty() || popupGeometry_.contentHeight <= 0.0f) {
        setScrollOffset(scrollOffset_);
        return;
    }
    const float itemTop = RowHeight * static_cast<float>(cursorIndex_);
    const float itemBottom = itemTop + RowHeight;
    float offset = scrollOffset_;
    if (itemTop < offset) {
        offset = itemTop;
    } else if (itemBottom > offset + popupGeometry_.contentHeight) {
        offset = itemBottom - popupGeometry_.contentHeight;
    }
    setScrollOffset(offset);
}

void DropBox::setExpandedState(bool expanded) {
    if (expanded_ == expanded) {
        return;
    }
    expanded_ = expanded;
    if (expanded_) {
        previousCanReceiveFocus_ = getCanReceiveFocus();
        focusabilityOverridden_ = true;
        setCanReceiveFocus(true);
        cursorIndex_ = selectedIndex_;
        scrollOffset_ = 0.0f;
        scrollTargetOffset_.reset();
        popupGeometryInitialized_ = false;
        syncPopupGeometry(true);
    } else if (focusabilityOverridden_) {
        setCanReceiveFocus(previousCanReceiveFocus_);
        focusabilityOverridden_ = false;
    }
    updateSelectionVisual();
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
    syncPopupGeometry(false);
    ensureCursorVisible();
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
    if (!getLocalBounds().contains(localPosition)) {
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
    syncPopupGeometry(false);
    const float viewportTop = popupGeometry_.positionY + ExpandedContentTop;
    if (localPosition.y < viewportTop ||
        localPosition.y >= viewportTop + popupGeometry_.contentHeight) {
        return std::nullopt;
    }
    const float relativeY = localPosition.y - viewportTop + scrollOffset_;
    const int index = static_cast<int>(relativeY / RowHeight);
    const int itemCount = static_cast<int>(items_.size());
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

bool DropBox::hasCanvasAncestor() const {
    std::shared_ptr<ControlBase> parent = getParent();
    while (parent != nullptr) {
        if (dynamic_cast<Canvas*>(parent.get()) != nullptr) {
            return true;
        }
        parent = parent->getParent();
    }
    return false;
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

bool DropBox::_ignoresAncestorInteractionClip() const {
    return expanded_;
}

bool DropBox::_hasOverlay() const {
    if (!expanded_) {
        return false;
    }
    syncPopupGeometry(false);
    return popupGeometry_.height > 0.0f;
}

void DropBox::_drawOverlay(sf::RenderTarget& target,
                           sf::RenderStates states) const {
    if (!expanded_ || !getVisible()) {
        return;
    }
    syncPopupGeometry(false);
    if (popupGeometry_.height <= 0.0f) {
        return;
    }
    ensurePopupVisuals();
    expandedWindow_->setPosition({0.0f, popupGeometry_.positionY});
    target.draw(*expandedWindow_, states);
    if (popupGeometry_.contentHeight <= 0.0f ||
        popupContentSprite_ == nullptr) {
        return;
    }
    renderPopupContent();
    states.blendMode = premultipliedRenderStates().blendMode;
    states.transform.translate(
        {0.0f, (popupGeometry_.positionY + ExpandedContentTop) * Scale});
    target.draw(*popupContentSprite_, states);
}

void DropBox::markVisualsDirty() {
    visualsDirty_ = true;
}

void DropBox::refreshDisplayScale() {
    markVisualsDirty();
    ControlBase::refreshDisplayScale();
}

void DropBox::releaseRuntimeCallbacks() noexcept {
    selectedIndexChangedCallback_ = {};
    selectionConfirmedCallback_ = {};
    expandedChangedCallback_ = {};
    ControlBase::releaseRuntimeCallbacks();
}

void DropBox::ensureVisuals() const {
    if (visualsDirty_) {
        rebuildVisuals();
    }
}

void DropBox::rebuildVisuals() const {
    const sf::Vector2i collapsedPixels = roundedSize(collapsedSize_);
    collapsedFrame_ = std::make_unique<Rect>(
        sf::IntRect({0, 0}, collapsedPixels), windowSkin_);
    collapsedText_ =
        std::make_unique<PlainText>(textConfig_, getSelectedItem());
    const int selectionWidth = std::max(0, collapsedPixels.x - 64);
    selectionRect_ =
        std::make_unique<Rect>(sf::IntRect({32, 0}, {selectionWidth, 32}),
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
    expandedWindow_.reset();
    popupContentCanvas_.reset();
    popupContentSprite_.reset();
    popupWindowSize_ = {};
    popupContentTextureSize_ = {};
    visualsDirty_ = false;
}

void DropBox::ensurePopupVisuals() const {
    ensureVisuals();
    const sf::Vector2i popupSize =
        roundedSize({collapsedSize_.x, popupGeometry_.height});
    const sf::Vector2u windowSize(static_cast<unsigned int>(popupSize.x),
                                  static_cast<unsigned int>(popupSize.y));
    if (expandedWindow_ == nullptr || popupWindowSize_ != windowSize) {
        expandedWindow_ = std::make_unique<Window>(
            sf::IntRect({0, 0}, popupSize), windowSkin_, repeated_);
        popupWindowSize_ = windowSize;
    }

    const sf::Vector2u contentTextureSize =
        ludork::engine::drop_box_impl::popupTextureSize(
            collapsedSize_.x, popupGeometry_.contentHeight, Scale);
    if (contentTextureSize.x == 0U || contentTextureSize.y == 0U) {
        popupContentCanvas_.reset();
        popupContentSprite_.reset();
        popupContentTextureSize_ = {};
        return;
    }
    if (popupContentCanvas_ == nullptr ||
        popupContentTextureSize_ != contentTextureSize) {
        popupContentCanvas_ = std::make_unique<sf::RenderTexture>(
            nonZeroRenderTextureSize(contentTextureSize));
        popupContentSprite_ =
            std::make_unique<sf::Sprite>(popupContentCanvas_->getTexture());
        popupContentSprite_->setTextureRect(
            {{0, 0},
             {static_cast<int>(contentTextureSize.x),
              static_cast<int>(contentTextureSize.y)}});
        popupContentTextureSize_ = contentTextureSize;
    }
}

void DropBox::renderPopupContent() const {
    popupContentCanvas_->clear(sf::Color::Transparent);
    sf::RenderStates states = canvasRenderStates();
    states.transform.translate({0.0f, -scrollOffset_ * Scale});

    if (!items_.empty()) {
        if (ludork::engine::drop_box_impl::selectionIntersectsViewport(
                cursorIndex_, RowHeight, scrollOffset_,
                popupGeometry_.contentHeight)) {
            popupContentCanvas_->draw(*selectionRect_, states);
        }
        const auto [firstIndex, lastIndex] =
            ludork::engine::drop_box_impl::visibleItemRange(
                items_.size(), RowHeight, scrollOffset_,
                popupGeometry_.contentHeight);
        for (int index = firstIndex; index < lastIndex; ++index) {
            popupContentCanvas_->draw(
                *itemTexts_[static_cast<std::size_t>(index)], states);
        }
    }
    popupContentCanvas_->display();
}

void DropBox::updateSelectionVisual() const {
    if (selectionRect_ == nullptr) {
        return;
    }
    selectionRect_->setVisible(expanded_ && !items_.empty());
    selectionRect_->setPosition(
        {ItemHorizontalInset, RowHeight * static_cast<float>(cursorIndex_)});
}

void DropBox::positionCollapsedText() const {
    if (collapsedText_ == nullptr) {
        return;
    }
    const sf::FloatRect bounds = collapsedText_->getLocalBounds();
    collapsedText_->setPosition(
        ludork::engine::drop_box_impl::collapsedTextPosition(
            bounds, collapsedSize_, CollapsedTextInset));
}

void DropBox::positionItemText(PlainText& text, int index) const {
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition(ludork::engine::drop_box_impl::itemTextPosition(
        bounds, collapsedSize_, index, RowHeight));
}
