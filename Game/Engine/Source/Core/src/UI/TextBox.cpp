#include "TextBoxImpl.hpp"

#include <EngineState.hpp>
#include <Input/InputService.hpp>
#include <UI/PlainTextConfig.hpp>
#include <UnicodeText.hpp>
#include <Utils/Render.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

sf::Vector2f normalizedSize(const sf::Vector2f& size) {
    return {std::isfinite(size.x) ? std::max(0.0f, std::round(size.x)) : 0.0f,
            std::isfinite(size.y) ? std::max(0.0f, std::round(size.y)) : 0.0f};
}

std::size_t codepointIndex(const std::string& text, std::size_t offset) {
    return sf::String::fromUtf8(
               text.begin(), text.begin() + static_cast<std::ptrdiff_t>(
                                                std::min(offset, text.size())))
        .getSize();
}

}  // namespace

sf::Vector2f TextBox::Impl::contentSize() const {
    return {std::max(0.0f, size.x - 16.0f), std::max(0.0f, size.y - 8.0f)};
}

float TextBox::Impl::positionAt(std::size_t byteOffset) const {
    return text->getInsertionPosition(codepointIndex(displayText, byteOffset))
        .x;
}

TextBox::TextBox(const sf::Vector2f& size, const sf::Image& windowSkin,
                 std::shared_ptr<PlainTextConfig> textConfig,
                 const std::string& text)
    : impl_(std::make_unique<Impl>()) {
    impl_->size = normalizedSize(size);
    impl_->frame = std::make_unique<Rect>(
        sf::IntRect({0, 0}, sf::Vector2i(impl_->size)), windowSkin);
    impl_->text = std::make_unique<PlainText>(std::move(textConfig), "");
    impl_->state.text = ludork::standard::unicode::sanitizeSingleLine(text);
    impl_->state.anchor = impl_->state.caret = impl_->state.text.size();
    setCanReceiveFocus(true);
    updateLayout();
}

TextBox::~TextBox() {
    impl_->textChanged = {};
    impl_->editingChanged = {};
    finishEdit();
}

sf::Vector2f TextBox::getSize() const {
    return impl_->size;
}
sf::FloatRect TextBox::getLocalBounds() const {
    return {{0.0f, 0.0f}, impl_->size};
}

void TextBox::resize(const sf::Vector2f& size) {
    const sf::Vector2f normalized = normalizedSize(size);
    if (normalized == impl_->size) {
        return;
    }
    impl_->size = normalized;
    impl_->frame->resize(normalized);
    impl_->dirty = true;
    updateLayout();
}

void TextBox::setWindowSkin(const sf::Image& windowSkin) {
    impl_->frame->setWindowSkin(windowSkin);
}

void TextBox::setTextConfig(std::shared_ptr<PlainTextConfig> textConfig) {
    impl_->text = std::make_unique<PlainText>(std::move(textConfig), "");
    impl_->text->setPresentationColour(this, presentationColour());
    impl_->dirty = true;
    updateLayout();
}

std::string TextBox::getString() const {
    return impl_->state.text;
}

void TextBox::setString(const std::string& text) {
    const std::string value =
        ludork::standard::unicode::sanitizeSingleLine(text);
    if (isEditing()) {
        ludork::engine::text_input::service().setText(impl_->session, value);
        return;
    }
    if (impl_->state.text == value) {
        return;
    }
    impl_->state.text = value;
    impl_->state.anchor = impl_->state.caret = value.size();
    impl_->dirty = true;
    updateLayout();
    if (impl_->textChanged) {
        impl_->textChanged(value);
    }
}

bool TextBox::isEditing() const {
    return impl_->session != 0 &&
           ludork::engine::text_input::service().getState(impl_->session) !=
               nullptr;
}

bool TextBox::beginEdit() {
    if (isEditing()) {
        return true;
    }
    if (!isInteractionEnabled()) {
        return false;
    }
    requestKeyboardFocus();
    const std::weak_ptr<TextBox> owner =
        ludork::Cast<TextBox>(weak_from_this().lock());
    if (owner.expired()) {
        return false;
    }
    ludork::engine::text_input::Request request;
    request.state = impl_->state;
    request.title = impl_->title;
    request.confirmText = impl_->done;
    request.cancelText = impl_->cancel;
    request.caretRect = getAbsoluteBounds();
    impl_->session = ludork::engine::text_input::service().begin(
        request, [owner](const ludork::engine::text_input::State& state,
                         std::optional<bool> finished) {
            const std::shared_ptr<TextBox> field = owner.lock();
            if (field == nullptr) {
                return;
            }
            const bool changed = field->impl_->state.text != state.text;
            field->impl_->state = state;
            field->impl_->dirty = true;
            field->impl_->blinkTime = 0.0f;
            if (finished.has_value()) {
                field->impl_->session = 0;
                field->impl_->dragging = false;
            }
            field->updateLayout();
            if (changed && field->impl_->textChanged) {
                field->impl_->textChanged(state.text);
            }
            if (finished.has_value() && field->impl_->editingChanged) {
                field->impl_->editingChanged(false);
            }
        });
    if (impl_->session == 0) {
        return false;
    }
    impl_->blinkTime = 0.0f;
    updateCaretRect();
    if (impl_->editingChanged) {
        impl_->editingChanged(true);
    }
    return true;
}

void TextBox::finishEdit() {
    if (impl_->session != 0) {
        const ludork::engine::text_input::SessionId session =
            std::exchange(impl_->session, 0);
        ludork::engine::text_input::service().finish(session, true);
    }
}

void TextBox::cancelEdit() {
    if (impl_->session != 0) {
        const ludork::engine::text_input::SessionId session =
            std::exchange(impl_->session, 0);
        ludork::engine::text_input::service().finish(session, false);
    }
}

void TextBox::setInputDialogLabels(const std::string& title,
                                   const std::string& done,
                                   const std::string& cancel) {
    impl_->title = title;
    impl_->done = done;
    impl_->cancel = cancel;
}

void TextBox::setOnTextChanged(
    std::optional<std::function<void(const std::string&)>> callback) {
    impl_->textChanged = callback.has_value()
                             ? std::move(*callback)
                             : std::function<void(const std::string&)>{};
}

void TextBox::setOnEditingChanged(
    std::optional<std::function<void(bool)>> callback) {
    impl_->editingChanged = callback.has_value() ? std::move(*callback)
                                                 : std::function<void(bool)>{};
}

void TextBox::update(float deltaTime) {
    if (isEditing() && !isInteractionEnabled()) {
        finishEdit();
    }
    if (isEditing() && !ludork::engine::text_input::service().isModal() &&
        inputProvider() != nullptr && inputProvider()->isMouseButtonPressed()) {
        if (!getAbsoluteInteractionBounds().contains(
                sf::Vector2f(inputProvider()->getMousePosition()))) {
            finishEdit();
        }
    }
    FunctionalBase::update(deltaTime);
    if (impl_->dragging && isEditing() && inputProvider() != nullptr) {
        placeCaret(sf::Vector2f(inputProvider()->getMousePosition()), true);
    }
    if (impl_->dragging && inputProvider() != nullptr &&
        !inputProvider()->isMouseButtonDown(sf::Mouse::Button::Left)) {
        impl_->dragging = false;
    }
    impl_->blinkTime =
        std::fmod(impl_->blinkTime + std::max(0.0f, deltaTime), 1.0f);
    updateLayout();
    updateCaretRect();
}

void TextBox::onConfirm(const UiInputEventArguments& arguments) {
    beginEdit();
    FunctionalBase::onConfirm(arguments);
}

void TextBox::onCancel(const UiInputEventArguments& arguments) {
    if (isEditing()) {
        cancelEdit();
    } else {
        FunctionalBase::onCancel(arguments);
    }
}

void TextBox::onClick(const UiInputEventArguments& arguments) {
    if (!impl_->suppressClick) {
        beginEdit();
    }
    impl_->suppressClick = false;
    FunctionalBase::onClick(arguments);
}

bool TextBox::onMouseButtonDown(const UiInputEventArguments& arguments) {
    const bool accepted =
        arguments.button == sf::Mouse::Button::Left &&
        arguments.position.has_value() &&
        getAbsoluteInteractionBounds().contains(*arguments.position);
    impl_->suppressClick = accepted;
    if (accepted && beginEdit() &&
        !ludork::engine::text_input::service().isModal()) {
        const bool extend =
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
        placeCaret(*arguments.position, extend);
        impl_->dragging = true;
    }
    return FunctionalBase::onMouseButtonDown(arguments) || accepted;
}

void TextBox::onMouseMoved(const UiInputEventArguments& arguments) {
    if (impl_->dragging && isEditing() && arguments.position.has_value()) {
        placeCaret(*arguments.position, true);
    }
    FunctionalBase::onMouseMoved(arguments);
}

void TextBox::onKeyDown(const UiInputEventArguments& arguments) {
    if (isEditing()) {
        return;
    }
    InputService* input = ludork::Cast<InputService>(inputProvider());
    if (input != nullptr && ownsKeyboardCursorFocus()) {
        if (input->isActionTriggered(input->getConfirmKeys(), true)) {
            onConfirm(arguments);
            return;
        }
        if (input->isActionTriggered(input->getCancelKeys(), true)) {
            onCancel(arguments);
            return;
        }
    }
    FunctionalBase::onKeyDown(arguments);
}

void TextBox::onFocusLost() {
    finishEdit();
    FunctionalBase::onFocusLost();
}

void TextBox::onInteractionInvalidated() {
    finishEdit();
    impl_->dragging = false;
}

void TextBox::placeCaret(const sf::Vector2f& position, bool extend) {
    updateLayout();
    const sf::Vector2f local =
        screenRenderTransform().getInverse().transformPoint(position) /
        engineState().getScale();
    const float x = local.x - 8.0f + impl_->scrollX;
    float nearest = std::numeric_limits<float>::max();
    std::size_t offset = 0;
    for (std::size_t index = 0; index < impl_->positions.size(); ++index) {
        const float distance = std::abs(x - impl_->positions[index]);
        if (distance < nearest) {
            nearest = distance;
            offset = impl_->offsets[index];
        }
    }
    ludork::engine::text_input::service().setSelection(
        impl_->session, extend ? impl_->state.anchor : offset, offset);
}

void TextBox::updateLayout() {
    if (!impl_->dirty && impl_->displayScale == engineState().getScale()) {
        return;
    }
    impl_->displayScale = engineState().getScale();
    impl_->displayText = impl_->state.text;
    std::size_t caret = impl_->state.caret;
    const std::size_t first = std::min(impl_->state.anchor, impl_->state.caret);
    const std::size_t last = std::max(impl_->state.anchor, impl_->state.caret);
    if (!impl_->state.preedit.empty()) {
        impl_->displayText.replace(first, last - first, impl_->state.preedit);
        caret = first + impl_->state.preeditCaret;
    }
    impl_->text->setString(impl_->displayText);
    impl_->caretX = impl_->positionAt(caret);
    impl_->anchorX = impl_->positionAt(
        impl_->state.preedit.empty() ? impl_->state.anchor : caret);
    impl_->preeditStartX = impl_->positionAt(first);
    impl_->preeditEndX = impl_->positionAt(first + impl_->state.preedit.size());
    impl_->offsets =
        ludork::standard::unicode::graphemeOffsets(impl_->state.text);
    impl_->positions.clear();
    for (std::size_t offset : impl_->offsets) {
        const std::size_t displayedOffset =
            impl_->state.preedit.empty() || offset <= first
                ? offset
                : first + impl_->state.preedit.size() +
                      (offset >= last ? offset - last : 0);
        impl_->positions.push_back(impl_->positionAt(displayedOffset));
    }
    const sf::Vector2f area = impl_->contentSize();
    const float width = std::max(0.0f, area.x - 2.0f);
    if (impl_->caretX < impl_->scrollX) {
        impl_->scrollX = impl_->caretX;
    } else if (impl_->caretX > impl_->scrollX + width) {
        impl_->scrollX = impl_->caretX - width;
    }
    const sf::FloatRect bounds = impl_->text->getLocalBounds();
    const float end = std::max(bounds.position.x + bounds.size.x,
                               impl_->positionAt(impl_->displayText.size()));
    impl_->scrollX =
        std::clamp(impl_->scrollX, 0.0f, std::max(0.0f, end - width));
    impl_->text->setPosition(
        {-impl_->scrollX, (area.y - bounds.size.y) * 0.5f - bounds.position.y});
    const sf::Vector2u pixels{
        static_cast<unsigned int>(
            std::max(1L, std::lround(area.x * impl_->displayScale))),
        static_cast<unsigned int>(
            std::max(1L, std::lround(area.y * impl_->displayScale)))};
    if (impl_->viewport == nullptr) {
        impl_->viewport = std::make_unique<sf::RenderTexture>(pixels);
    } else if (impl_->viewport->getSize() != pixels &&
               !impl_->viewport->resize(pixels)) {
        throw std::runtime_error("Failed to resize TextBox viewport");
    }
    impl_->dirty = false;
}

void TextBox::updateCaretRect() {
    if (!isEditing()) {
        return;
    }
    const float height = static_cast<float>(impl_->text->getCharacterSize());
    const float y = (impl_->size.y - height) * 0.5f;
    const sf::FloatRect rectangle(
        {(8.0f + impl_->caretX - impl_->scrollX) * impl_->displayScale,
         y * impl_->displayScale},
        {impl_->displayScale, height * impl_->displayScale});
    ludork::engine::text_input::service().setCaretRect(
        impl_->session, screenRenderTransform().transformRect(rectangle));
}

void TextBox::refreshDisplayScale() {
    impl_->frame->refreshDisplayScale();
    impl_->text->refreshDisplayScale();
    impl_->dirty = true;
    updateLayout();
    ControlBase::refreshDisplayScale();
}

void TextBox::_refreshPresentationColour() {
    impl_->frame->setPresentationColour(this, presentationColour());
    impl_->text->setPresentationColour(this, presentationColour());
}

void TextBox::releaseRuntimeCallbacks() noexcept {
    impl_->textChanged = {};
    impl_->editingChanged = {};
    finishEdit();
    ControlBase::releaseRuntimeCallbacks();
}

void TextBox::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (!getVisible()) {
        return;
    }
    const_cast<TextBox*>(this)->updateLayout();
    _applyRenderStates(states);
    target.draw(*impl_->frame, states);
    const sf::Vector2f area = impl_->contentSize();
    if (area.x <= 0.0f || area.y <= 0.0f) {
        return;
    }
    impl_->viewport->clear(sf::Color::Transparent);
    const float scale = impl_->displayScale;
    const float height =
        std::min(area.y, static_cast<float>(impl_->text->getCharacterSize()));
    const float top = (area.y - height) * 0.5f;
    if (isEditing() && impl_->state.preedit.empty() &&
        impl_->state.anchor != impl_->state.caret) {
        const float left = std::min(impl_->anchorX, impl_->caretX);
        sf::RectangleShape selection(
            {std::abs(impl_->caretX - impl_->anchorX) * scale, height * scale});
        selection.setPosition({(left - impl_->scrollX) * scale, top * scale});
        selection.setFillColor(
            modulatePresentationColour(sf::Color(72, 114, 172, 180)));
        impl_->viewport->draw(selection);
    }
    impl_->viewport->draw(*impl_->text);
    if (isEditing() && !impl_->state.preedit.empty()) {
        sf::RectangleShape underline(
            {std::abs(impl_->preeditEndX - impl_->preeditStartX) * scale,
             scale});
        underline.setPosition(
            {(std::min(impl_->preeditStartX, impl_->preeditEndX) -
              impl_->scrollX) *
                 scale,
             (top + height - 1.0f) * scale});
        underline.setFillColor(modulatePresentationColour(sf::Color::White));
        impl_->viewport->draw(underline);
    }
    if (isEditing() && impl_->blinkTime < 0.5f) {
        sf::RectangleShape cursor({std::max(1.0f, scale), height * scale});
        cursor.setPosition(
            {(impl_->caretX - impl_->scrollX) * scale, top * scale});
        cursor.setFillColor(modulatePresentationColour(sf::Color::White));
        impl_->viewport->draw(cursor);
    }
    impl_->viewport->display();
    sf::Sprite content(impl_->viewport->getTexture());
    content.setPosition({8.0f * scale, 4.0f * scale});
    states.blendMode = premultipliedRenderStates().blendMode;
    target.draw(content, states);
}
