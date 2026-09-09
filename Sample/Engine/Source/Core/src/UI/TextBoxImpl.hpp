#pragma once

#include <UI/TextBox.hpp>
#include <UI/PlainText.hpp>
#include <UI/Rect.hpp>
#include <Input/TextInputService.hpp>

struct TextBox::Impl {
    sf::Vector2f size;
    std::unique_ptr<Rect> frame;
    std::unique_ptr<PlainText> text;
    mutable std::unique_ptr<sf::RenderTexture> viewport;
    ludork::engine::text_input::State state;
    ludork::engine::text_input::SessionId session = 0;
    std::string title = "Text input";
    std::string done = "Done";
    std::string cancel = "Cancel";
    std::function<void(const std::string&)> textChanged;
    std::function<void(bool)> editingChanged;
    std::string displayText;
    std::vector<std::size_t> offsets;
    std::vector<float> positions;
    float caretX = 0.0f;
    float anchorX = 0.0f;
    float preeditStartX = 0.0f;
    float preeditEndX = 0.0f;
    float scrollX = 0.0f;
    float blinkTime = 0.0f;
    float displayScale = 1.0f;
    bool dragging = false;
    bool suppressClick = false;
    bool dirty = true;

    sf::Vector2f contentSize() const;
    float positionAt(std::size_t byteOffset) const;
};
