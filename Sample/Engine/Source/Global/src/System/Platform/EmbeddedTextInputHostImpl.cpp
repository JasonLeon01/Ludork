#include "EmbeddedTextInputHostImpl.hpp"

#include <EditorCommandServices.hpp>
#include <Runtime/Json.hpp>
#include <cmath>
#include <string>
#include <utility>

namespace ludork::global {

EmbeddedTextInputHostImpl::~EmbeddedTextInputHostImpl() {
    end(id_);
}

bool EmbeddedTextInputHostImpl::isAvailable() const {
    return connectionId_ != 0 &&
           ludork::standard::editorConnectionId() == connectionId_;
}

bool EmbeddedTextInputHostImpl::isModal() const {
    return false;
}

bool EmbeddedTextInputHostImpl::handlesKeyboard() const {
    return false;
}

bool EmbeddedTextInputHostImpl::begin(
    ludork::engine::text_input::SessionId id,
    const ludork::engine::text_input::Request& request, Sink) {
    id_ = id;
    connectionId_ = ludork::standard::editorConnectionId();
    caretRect_ = request.caretRect;
    return send("begin", &caretRect_);
}

void EmbeddedTextInputHostImpl::update(ludork::engine::text_input::SessionId id,
                                       const ludork::engine::text_input::State&,
                                       const sf::FloatRect& caretRect) {
    if (id == 0 || id != id_ || caretRect == caretRect_) {
        return;
    }
    caretRect_ = caretRect;
    if (!send("update", &caretRect_)) {
        connectionId_ = 0;
    }
}

void EmbeddedTextInputHostImpl::end(ludork::engine::text_input::SessionId id) {
    if (id == 0 || id != id_) {
        return;
    }
    send("end", nullptr);
    id_ = 0;
    connectionId_ = 0;
}

bool EmbeddedTextInputHostImpl::send(std::string_view action,
                                     const sf::FloatRect* caretRect) {
    RuntimeData::Map message{
        {"v", RuntimeData(std::int64_t{1})},
        {"type", RuntimeData("textInput")},
        {"action", RuntimeData(std::string(action))},
        {"session", RuntimeData(std::to_string(id_))},
    };
    if (caretRect != nullptr) {
        if (!std::isfinite(caretRect->position.x) ||
            !std::isfinite(caretRect->position.y) ||
            !std::isfinite(caretRect->size.x) ||
            !std::isfinite(caretRect->size.y)) {
            return false;
        }
        message.emplace(
            "x", RuntimeData(static_cast<double>(caretRect->position.x)));
        message.emplace(
            "y", RuntimeData(static_cast<double>(caretRect->position.y)));
        message.emplace("width",
                        RuntimeData(static_cast<double>(caretRect->size.x)));
        message.emplace("height",
                        RuntimeData(static_cast<double>(caretRect->size.y)));
    }
    return ludork::standard::sendEditorMessage(
        connectionId_, stringifyJSON(RuntimeData(std::move(message))));
}

}  // namespace ludork::global
