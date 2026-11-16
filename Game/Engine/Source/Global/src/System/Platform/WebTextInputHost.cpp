#if defined(__EMSCRIPTEN__)
#include "WebTextInputHostImpl.hpp"

#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <emscripten.h>

#include <cstdlib>
#include <utility>

namespace ludork::global {
namespace {

EM_JS_DEPS(webTextInputDependencies, "$stringToNewUTF8");
// clang-format off
EM_JS(bool, showWebTextInput, (const char* selector, const char* requestJson), {
    var canvas = document.querySelector(UTF8ToString(selector));
    if (!canvas) return false;
    Module['ludorkTextInput']?.dispose();
    var request = JSON.parse(UTF8ToString(requestJson));
    var encoder = new TextEncoder();
    var decoder = new TextDecoder();
    var byteOffset = (text, offset) => encoder.encode(text.slice(0, offset)).length;
    var textOffset = (text, offset) => decoder.decode(encoder.encode(text).slice(0, offset)).length;
    var overlay = document.createElement('div');
    overlay.style.cssText = 'position:fixed;inset:0;z-index:2147483647;background:#0009;display:flex;align-items:center;justify-content:center;padding:24px;box-sizing:border-box;';
    var dialog = document.createElement('section');
    dialog.setAttribute('role', 'dialog');
    dialog.setAttribute('aria-modal', 'true');
    dialog.style.cssText = 'font:16px system-ui,sans-serif;color:#f5f5f5;background:#24272d;border:1px solid #626873;border-radius:10px;padding:24px;width:420px;max-width:100%;max-height:calc(100dvh - 48px);overflow:auto;box-sizing:border-box;';
    var title = document.createElement('h2');
    title.textContent = request.title;
    title.style.cssText = 'margin:0 0 12px;font-size:20px;';
    var prompt = document.createElement('p');
    prompt.textContent = request.prompt;
    prompt.style.cssText = 'margin:0 0 12px;white-space:pre-wrap;';
    prompt.hidden = !request.prompt;
    var input = document.createElement('input');
    input.type = 'text';
    input.value = request.text;
    input.placeholder = request.placeholder;
    input.setAttribute('aria-label', request.title || request.placeholder);
    input.autocomplete = 'off';
    input.spellcheck = false;
    input.style.cssText = 'width:100%;box-sizing:border-box;font:inherit;color:#fff;background:#15181d;border:1px solid #a0a7b2;border-radius:4px;padding:10px;';
    var buttons = document.createElement('div');
    buttons.style.cssText = 'display:flex;justify-content:flex-end;gap:12px;margin-top:18px;';
    var confirm = document.createElement('button');
    var cancel = document.createElement('button');
    confirm.textContent = request.confirmText;
    cancel.textContent = request.cancelText;
    for (var button of [confirm, cancel]) button.style.cssText = 'font:inherit;padding:8px 18px;min-height:40px;border-radius:4px;border:1px solid #a0a7b2;color:#fff;background:#353b46;cursor:pointer;';
    buttons.append(confirm, cancel);
    dialog.append(title, prompt, input, buttons);
    overlay.append(dialog);
    document.body.append(overlay);
    var state = { id: request.id, canvas, overlay, input, events: [], composing: false, completed: false, base: null, preedit: "", dispose: null, textOffset };
    Module['ludorkTextInput'] = state;
    var live = () => Module['ludorkTextInput'] === state && !state.completed;
    var snapshot = () => {
        var start = input.selectionStart ?? 0;
        var end = input.selectionEnd ?? start;
        var backward = input.selectionDirection === 'backward';
        return { text: input.value, anchor: byteOffset(input.value, backward ? end : start), caret: byteOffset(input.value, backward ? start : end), preedit: "", preeditCaret: 0 };
    };
    var changed = () => {
        if (!live()) return;
        var value;
        if (state.composing) {
            value = { ...state.base, preedit: state.preedit, preeditCaret: encoder.encode(state.preedit).length };
        } else value = snapshot();
        state.events.push({ kind: 'replace', ...value });
    };
    var complete = accepted => {
        if (!live()) return;
        state.completed = true;
        state.events.push({ kind: 'complete', accepted, ...snapshot() });
    };
    input.addEventListener('compositionstart', () => {
        if (!live()) return;
        state.base = snapshot();
        state.composing = true;
        state.preedit = "";
    });
    input.addEventListener('compositionupdate', event => {
        if (!live()) return;
        state.preedit = event.data || "";
        changed();
    });
    input.addEventListener('compositionend', () => {
        if (!live()) return;
        state.composing = false;
        state.preedit = "";
        changed();
    });
    input.addEventListener('input', event => {
        if (state.composing && event.data !== null) state.preedit = event.data;
        changed();
    });
    input.addEventListener('select', () => { if (!state.composing) changed(); });
    dialog.addEventListener('keydown', event => {
        event.stopPropagation();
        if (event.isComposing || state.composing || event.keyCode === 229) return;
        if (event.key === 'Enter') { event.preventDefault(); complete(true); }
        else if (event.key === 'Escape') { event.preventDefault(); complete(false); }
    });
    dialog.addEventListener('keyup', event => event.stopPropagation());
    confirm.addEventListener('click', () => complete(true));
    cancel.addEventListener('click', () => complete(false));
    var hidden = () => { if (document.hidden) complete(false); };
    document.addEventListener('visibilitychange', hidden);
    state.dispose = () => {
        state.completed = true;
        document.removeEventListener('visibilitychange', hidden);
        overlay.remove();
    };
    input.focus({ preventScroll: true });
    input.setSelectionRange(textOffset(request.text, Math.min(request.anchor, request.caret)), textOffset(request.text, Math.max(request.anchor, request.caret)), request.anchor > request.caret ? 'backward' : 'forward');
    return true;
});

EM_JS(bool, webTextInputAvailable, (const char* id), {
    var state = Module['ludorkTextInput'];
    return state?.id === UTF8ToString(id) && state.overlay.isConnected;
});

EM_JS(char*, takeWebTextInputEvent, (const char* id), {
    var state = Module['ludorkTextInput'];
    if (state?.id !== UTF8ToString(id) || !state.events.length) return 0;
    return stringToNewUTF8(JSON.stringify(state.events.shift()));
});

EM_JS(void, updateWebTextInput, (const char* id, const char* stateJson), {
    var state = Module['ludorkTextInput'];
    if (state?.id !== UTF8ToString(id) || state.completed || state.composing || state.events.length) return;
    var value = JSON.parse(UTF8ToString(stateJson));
    if (value.preedit) return;
    if (state.input.value !== value.text) state.input.value = value.text;
    var start = state.textOffset(value.text, Math.min(value.anchor, value.caret));
    var end = state.textOffset(value.text, Math.max(value.anchor, value.caret));
    var direction = value.anchor > value.caret ? 'backward' : 'forward';
    if (state.input.selectionStart !== start || state.input.selectionEnd !== end || state.input.selectionDirection !== direction) state.input.setSelectionRange(start, end, direction);
});

EM_JS(void, closeWebTextInput, (const char* id), {
    var state = Module['ludorkTextInput'];
    if (state?.id !== UTF8ToString(id)) return;
    state.dispose();
    delete Module['ludorkTextInput'];
    if (state.canvas.isConnected) state.canvas.focus({ preventScroll: true });
});
// clang-format on

RuntimeData::Map stateData(const ludork::engine::text_input::State& state) {
    return {{"text", RuntimeData(state.text)},
            {"anchor", RuntimeData(static_cast<std::int64_t>(state.anchor))},
            {"caret", RuntimeData(static_cast<std::int64_t>(state.caret))},
            {"preedit", RuntimeData(state.preedit)},
            {"preeditCaret",
             RuntimeData(static_cast<std::int64_t>(state.preeditCaret))}};
}

}  // namespace

WebTextInputHostImpl::WebTextInputHostImpl(sf::WindowHandle window)
    : canvas_(window) {}

WebTextInputHostImpl::~WebTextInputHostImpl() {
    end(id_);
}

bool WebTextInputHostImpl::isAvailable() const {
    const std::string id = std::to_string(id_);
    if (id_ == 0 || !webTextInputAvailable(id.c_str())) {
        return false;
    }
    while (true) {
        const std::unique_ptr<char, decltype(&std::free)> json(
            takeWebTextInputEvent(id.c_str()), std::free);
        if (!json) {
            break;
        }
        const RuntimeData data = parseJSONText(json.get());
        const RuntimeData::Map& value =
            ludork::runtime::value_reader::requireMap(data,
                                                      "Browser text input");
        const auto text = [&value](const char* key) {
            return ludork::runtime::value_reader::requireString(value.at(key),
                                                                key);
        };
        const auto offset = [&value](const char* key) {
            return static_cast<std::size_t>(
                ludork::runtime::value_reader::requireInteger(value.at(key),
                                                              key));
        };
        ludork::engine::text_input::Event event;
        event.kind = text("kind") == "complete"
                         ? ludork::engine::text_input::Event::Kind::Complete
                         : ludork::engine::text_input::Event::Kind::Replace;
        event.state = {text("text"), offset("anchor"), offset("caret"),
                       text("preedit"), offset("preeditCaret")};
        if (event.kind == ludork::engine::text_input::Event::Kind::Complete) {
            event.accepted = ludork::runtime::value_reader::requireBool(
                value.at("accepted"), "Text input completion");
        }
        sink_(id_, std::move(event));
    }
    return true;
}

bool WebTextInputHostImpl::isModal() const {
    return true;
}
bool WebTextInputHostImpl::handlesKeyboard() const {
    return true;
}

bool WebTextInputHostImpl::begin(
    ludork::engine::text_input::SessionId id,
    const ludork::engine::text_input::Request& request, Sink sink) {
    end(id_);
    RuntimeData::Map value = stateData(request.state);
    value.emplace("id", RuntimeData(std::to_string(id)));
    value.emplace("title", RuntimeData(request.title));
    value.emplace("prompt", RuntimeData(request.prompt));
    value.emplace("placeholder", RuntimeData(request.placeholder));
    value.emplace("confirmText", RuntimeData(request.confirmText));
    value.emplace("cancelText", RuntimeData(request.cancelText));
    if (!showWebTextInput(
            canvas_.c_str(),
            stringifyJSON(RuntimeData(std::move(value))).c_str())) {
        return false;
    }
    id_ = id;
    sink_ = std::move(sink);
    return true;
}

void WebTextInputHostImpl::update(
    ludork::engine::text_input::SessionId id,
    const ludork::engine::text_input::State& state, const sf::FloatRect&) {
    if (id == id_ && id != 0) {
        updateWebTextInput(
            std::to_string(id).c_str(),
            stringifyJSON(RuntimeData(stateData(state))).c_str());
    }
}

void WebTextInputHostImpl::end(ludork::engine::text_input::SessionId id) {
    if (id == 0 || id != id_) {
        return;
    }
    closeWebTextInput(std::to_string(id).c_str());
    id_ = 0;
    sink_ = {};
}

std::shared_ptr<ludork::engine::text_input::TextInputHost>
createWebTextInputHost(sf::WindowHandle window) {
    return std::make_shared<WebTextInputHostImpl>(window);
}

}  // namespace ludork::global
#endif
