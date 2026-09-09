#pragma once

#include <EngineRuntimeApi.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace ludork::engine::text_input {

using SessionId = std::uint64_t;

struct State {
    std::string text;
    std::size_t anchor = 0;
    std::size_t caret = 0;
    std::string preedit;
    std::size_t preeditCaret = 0;
};

struct Request {
    State state;
    sf::FloatRect caretRect;
    std::string title;
    std::string prompt;
    std::string placeholder;
    std::string confirmText;
    std::string cancelText;
};

enum class Command {
    Left,
    Right,
    Home,
    End,
    Backspace,
    Delete,
    SelectAll,
    Copy,
    Cut,
    Paste,
    Finish,
    Cancel
};

struct Event {
    enum class Kind {
        Replace,
        Insert,
        Preedit,
        Command,
        Complete
    };
    Kind kind = Kind::Replace;
    State state;
    std::string text;
    std::optional<std::pair<std::size_t, std::size_t>> replacementSelection;
    Command command = Command::Finish;
    bool extendSelection = false;
    bool accepted = true;
};

class LUDORK_ENGINE_API TextInputHost {
public:
    using Sink = std::function<void(SessionId, Event)>;
    virtual ~TextInputHost() = default;
    virtual bool isModal() const = 0;
    virtual bool handlesKeyboard() const = 0;
    virtual bool begin(SessionId id, const Request& request, Sink sink) = 0;
    virtual void update(SessionId id, const State& state,
                        const sf::FloatRect& caretRect) = 0;
    virtual void end(SessionId id) = 0;
};

}  // namespace ludork::engine::text_input
