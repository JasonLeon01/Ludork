#include <Input/TextInputService.hpp>
#include "TextInputServiceImpl.hpp"

#include <UnicodeText.hpp>
#include <SFML/System/String.hpp>
#include <SFML/Window/Clipboard.hpp>
#include <algorithm>
#include <utility>

namespace ludork::engine::text_input {

namespace {

std::size_t boundary(const std::string& text, std::size_t offset) {
    const std::vector<std::size_t> offsets =
        ludork::standard::unicode::graphemeOffsets(text);
    const auto next = std::upper_bound(offsets.begin(), offsets.end(), offset);
    return next == offsets.begin() ? 0 : *std::prev(next);
}

std::string utf8(const sf::String& value) {
    const sf::U8String bytes = value.toUtf8();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

}  // namespace

void TextInputService::Impl::normalize() {
    state.text = ludork::standard::unicode::sanitizeSingleLine(state.text);
    state.anchor = boundary(state.text, state.anchor);
    state.caret = boundary(state.text, state.caret);
    state.preedit =
        ludork::standard::unicode::sanitizeSingleLine(state.preedit);
    state.preeditCaret = boundary(state.preedit, state.preeditCaret);
}

void TextInputService::Impl::syncHost() {
    if (host != nullptr && id != 0) {
        host->update(id, state, caretRect);
    }
}

void TextInputService::Impl::notify() {
    syncHost();
    if (callback) {
        const Callback currentCallback = callback;
        const State snapshot = state;
        currentCallback(snapshot, std::nullopt);
    }
}

void TextInputService::Impl::insert(const std::string& value) {
    const std::size_t begin = std::min(state.anchor, state.caret);
    const std::size_t end = std::max(state.anchor, state.caret);
    const std::string sanitized =
        ludork::standard::unicode::sanitizeSingleLine(value);
    state.text.replace(begin, end - begin, sanitized);
    state.caret = state.anchor = boundary(state.text, begin + sanitized.size());
    state.preedit.clear();
    state.preeditCaret = 0;
    notify();
}

TextInputService::TextInputService() : impl_(std::make_unique<Impl>()) {}
TextInputService::~TextInputService() = default;

void TextInputService::setHost(std::shared_ptr<TextInputHost> host) {
    close();
    impl_->host = std::move(host);
}

SessionId TextInputService::begin(const Request& request, Callback callback) {
    close();
    impl_->id = ++impl_->nextId;
    impl_->state = request.state;
    impl_->normalize();
    impl_->original = impl_->state;
    impl_->caretRect = request.caretRect;
    impl_->callback = std::move(callback);
    impl_->blocked = true;
    impl_->composing = false;
    impl_->compositionInFrame = false;
    impl_->activationKeys.clear();
    for (sf::Keyboard::Key key :
         {sf::Keyboard::Key::Enter, sf::Keyboard::Key::Space}) {
        if (impl_->heldKeys.contains(key) || sf::Keyboard::isKeyPressed(key)) {
            impl_->activationKeys.insert(key);
        }
    }
    if (impl_->host != nullptr) {
        Request normalized = request;
        normalized.state = impl_->state;
        const std::weak_ptr<Impl::Queue> queue = impl_->queue;
        if (!impl_->host->begin(
                impl_->id, normalized, [queue](SessionId id, Event event) {
                    if (const auto pending = queue.lock()) {
                        const std::lock_guard<std::mutex> lock(pending->mutex);
                        pending->events.emplace_back(id, std::move(event));
                    }
                })) {
            impl_->host->end(impl_->id);
            impl_->id = 0;
            impl_->callback = {};
            return 0;
        }
    }
    return impl_->id;
}

void TextInputService::finish(SessionId id, bool accepted) {
    if (id == 0 || id != impl_->id) {
        return;
    }
    const Callback callback = std::move(impl_->callback);
    State result = accepted ? impl_->state : impl_->original;
    result.preedit.clear();
    result.preeditCaret = 0;
    impl_->id = 0;
    impl_->composing = false;
    impl_->blocked = true;
    impl_->releaseFrames = 1;
    for (int code = 0; code < static_cast<int>(sf::Keyboard::KeyCount);
         ++code) {
        const sf::Keyboard::Key key = static_cast<sf::Keyboard::Key>(code);
        if (sf::Keyboard::isKeyPressed(key)) {
            impl_->releaseKeys.insert(key);
        }
    }
    if (impl_->host != nullptr) {
        impl_->host->end(id);
    }
    impl_->state = result;
    if (callback) {
        callback(result, accepted);
    }
}

void TextInputService::setText(SessionId id, const std::string& text) {
    if (id == 0 || id != impl_->id) {
        return;
    }
    impl_->state.text = text;
    impl_->state.preedit.clear();
    impl_->state.preeditCaret = 0;
    impl_->normalize();
    impl_->notify();
}

void TextInputService::setSelection(SessionId id, std::size_t anchor,
                                    std::size_t caret) {
    if (id == 0 || id != impl_->id) {
        return;
    }
    impl_->state.anchor = boundary(impl_->state.text, anchor);
    impl_->state.caret = boundary(impl_->state.text, caret);
    impl_->state.preedit.clear();
    impl_->state.preeditCaret = 0;
    impl_->notify();
}

void TextInputService::setCaretRect(SessionId id, const sf::FloatRect& rect) {
    if (id == 0 || id != impl_->id || impl_->caretRect == rect) {
        return;
    }
    impl_->caretRect = rect;
    impl_->syncHost();
}

void TextInputService::setPreedit(SessionId id, const std::string& text,
                                  std::size_t caret) {
    if (id == 0 || id != impl_->id) {
        return;
    }
    impl_->compositionInFrame = impl_->compositionInFrame || !text.empty();
    impl_->state.preedit = text;
    impl_->state.preeditCaret = caret;
    impl_->normalize();
    impl_->notify();
}

void TextInputService::setComposing(SessionId id, bool composing) {
    if (id == 0 || id != impl_->id) {
        return;
    }
    impl_->composing = composing;
    impl_->compositionInFrame = impl_->compositionInFrame || composing;
    if (!composing) {
        setPreedit(id, {}, 0);
    }
}

const State* TextInputService::getState(SessionId id) const {
    return id != 0 && id == impl_->id ? &impl_->state : nullptr;
}

bool TextInputService::isEditing() const {
    return impl_->id != 0;
}
bool TextInputService::isModal() const {
    return isEditing() && impl_->host != nullptr && impl_->host->isModal();
}
bool TextInputService::blocksGameplay() const {
    return isEditing() || impl_->blocked;
}

void TextInputService::beginFrame() {
    impl_->compositionInFrame =
        impl_->composing || !impl_->state.preedit.empty();
    std::erase_if(impl_->releaseKeys, [](sf::Keyboard::Key key) {
        return !sf::Keyboard::isKeyPressed(key);
    });
    impl_->blocked =
        isEditing() || impl_->releaseFrames > 0 || !impl_->releaseKeys.empty();
    if (impl_->releaseFrames > 0) {
        --impl_->releaseFrames;
    }
    pump();
}

void TextInputService::pump() {
    if (isEditing() && impl_->host != nullptr && !impl_->host->isAvailable()) {
        close();
    }
    std::deque<std::pair<SessionId, Event>> events;
    {
        const std::lock_guard<std::mutex> lock(impl_->queue->mutex);
        events.swap(impl_->queue->events);
    }
    for (const auto& [id, event] : events) {
        if (id == 0 || id != impl_->id) {
            continue;
        }
        if (event.replacementSelection.has_value()) {
            impl_->state.anchor =
                boundary(impl_->state.text, event.replacementSelection->first);
            impl_->state.caret =
                boundary(impl_->state.text, event.replacementSelection->second);
        }
        switch (event.kind) {
            case Event::Kind::Complete:
                if (event.accepted) {
                    impl_->state = event.state;
                    impl_->normalize();
                }
                finish(id, event.accepted);
                break;
            case Event::Kind::Replace:
                impl_->state = event.state;
                impl_->normalize();
                impl_->notify();
                break;
            case Event::Kind::Insert:
                impl_->insert(event.text);
                break;
            case Event::Kind::Preedit:
                setPreedit(id, event.state.preedit, event.state.preeditCaret);
                break;
            case Event::Kind::Command:
                execute(event.command, event.extendSelection);
                break;
        }
    }
}

bool TextInputService::processEvent(const sf::Event& event) {
    pump();
    if (const auto* key = event.getIf<sf::Event::KeyReleased>()) {
        impl_->heldKeys.erase(key->code);
        impl_->activationKeys.erase(key->code);
    } else if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        impl_->heldKeys.insert(key->code);
    }
    if (event.is<sf::Event::FocusLost>() || event.is<sf::Event::Closed>()) {
        if (!isModal() || event.is<sf::Event::Closed>()) {
            close();
        }
        return false;
    }
    const bool keyboard = event.is<sf::Event::KeyPressed>() ||
                          event.is<sf::Event::KeyReleased>() ||
                          event.is<sf::Event::TextEntered>();
    const bool gamepad = event.is<sf::Event::JoystickButtonPressed>() ||
                         event.is<sf::Event::JoystickButtonReleased>() ||
                         event.is<sf::Event::JoystickMoved>();
    if (!blocksGameplay()) {
        return false;
    }
    if (!isEditing() || isModal() ||
        (impl_->host != nullptr && impl_->host->handlesKeyboard())) {
        return keyboard || gamepad;
    }
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        if (impl_->activationKeys.contains(key->code)) {
            return true;
        }
#if defined(__APPLE__)
        const bool shortcut = key->system;
#else
        const bool shortcut = key->control;
#endif
        if (shortcut) {
            if (key->code == sf::Keyboard::Key::A) {
                execute(Command::SelectAll);
            }
            if (key->code == sf::Keyboard::Key::C) {
                execute(Command::Copy);
            }
            if (key->code == sf::Keyboard::Key::X) {
                execute(Command::Cut);
            }
            if (key->code == sf::Keyboard::Key::V) {
                execute(Command::Paste);
            }
            return true;
        }
        if (!impl_->state.preedit.empty() || impl_->compositionInFrame) {
            return true;
        }
        switch (key->code) {
            case sf::Keyboard::Key::Left:
                execute(Command::Left, key->shift);
                break;
            case sf::Keyboard::Key::Right:
                execute(Command::Right, key->shift);
                break;
            case sf::Keyboard::Key::Home:
                execute(Command::Home, key->shift);
                break;
            case sf::Keyboard::Key::End:
                execute(Command::End, key->shift);
                break;
            case sf::Keyboard::Key::Backspace:
                execute(Command::Backspace);
                break;
            case sf::Keyboard::Key::Delete:
                execute(Command::Delete);
                break;
            case sf::Keyboard::Key::Enter:
                execute(Command::Finish);
                break;
            case sf::Keyboard::Key::Escape:
                execute(Command::Cancel);
                break;
            default:
                break;
        }
    } else if (const auto* text = event.getIf<sf::Event::TextEntered>()) {
        if (text->unicode == U' ' &&
            impl_->activationKeys.contains(sf::Keyboard::Key::Space)) {
            return true;
        }
        if (text->unicode >= 32 && text->unicode != 127 &&
            text->unicode <= 0x10FFFF &&
            !(text->unicode >= 0xD800 && text->unicode <= 0xDFFF)) {
            impl_->insert(utf8(sf::String(text->unicode)));
        }
    }
    return keyboard || gamepad;
}

void TextInputService::execute(Command command, bool extendSelection) {
    if (!isEditing()) {
        return;
    }
    State& state = impl_->state;
    const std::vector<std::size_t> offsets =
        ludork::standard::unicode::graphemeOffsets(state.text);
    const auto at =
        std::lower_bound(offsets.begin(), offsets.end(), state.caret);
    const std::size_t previous = at == offsets.begin() ? 0 : *std::prev(at);
    const std::size_t next =
        at == offsets.end() || std::next(at) == offsets.end()
            ? state.text.size()
            : *std::next(at);
    const std::size_t first = std::min(state.anchor, state.caret);
    const std::size_t last = std::max(state.anchor, state.caret);
    switch (command) {
        case Command::Finish:
            finish(impl_->id, true);
            return;
        case Command::Cancel:
            finish(impl_->id, false);
            return;
        case Command::Copy:
        case Command::Cut:
            if (first != last) {
                const std::string selection =
                    state.text.substr(first, last - first);
                sf::Clipboard::setString(
                    sf::String::fromUtf8(selection.begin(), selection.end()));
                if (command == Command::Cut) {
                    impl_->insert({});
                }
            }
            return;
        case Command::Paste:
            impl_->insert(utf8(sf::Clipboard::getString()));
            return;
        case Command::SelectAll:
            state.anchor = 0;
            state.caret = state.text.size();
            break;
        case Command::Left:
            state.caret = !extendSelection && first != last ? first : previous;
            if (!extendSelection) {
                state.anchor = state.caret;
            }
            break;
        case Command::Right:
            state.caret = !extendSelection && first != last ? last : next;
            if (!extendSelection) {
                state.anchor = state.caret;
            }
            break;
        case Command::Home:
            state.caret = 0;
            if (!extendSelection) {
                state.anchor = state.caret;
            }
            break;
        case Command::End:
            state.caret = state.text.size();
            if (!extendSelection) {
                state.anchor = state.caret;
            }
            break;
        case Command::Backspace:
            if (first == last) {
                state.anchor = previous;
            }
            impl_->insert({});
            return;
        case Command::Delete:
            if (first == last) {
                state.anchor = next;
            }
            impl_->insert({});
            return;
    }
    state.preedit.clear();
    state.preeditCaret = 0;
    impl_->notify();
}

void TextInputService::close() {
    finish(impl_->id, true);
}
void TextInputService::shutdown() {
    close();
    impl_->host.reset();
    impl_->heldKeys.clear();
    impl_->activationKeys.clear();
    impl_->releaseKeys.clear();
    impl_->blocked = false;
    impl_->releaseFrames = 0;
    const std::lock_guard<std::mutex> lock(impl_->queue->mutex);
    impl_->queue->events.clear();
}

TextInputService& service() {
    static TextInputService instance;
    return instance;
}

}  // namespace ludork::engine::text_input
