#pragma once

#include <Input/TextInputHost.hpp>
#include <SFML/Window/Event.hpp>
#include <memory>

namespace ludork::engine::text_input {

class LUDORK_ENGINE_API TextInputService {
public:
    using Callback = std::function<void(const State&, std::optional<bool>)>;
    TextInputService();
    ~TextInputService();
    void setHost(std::shared_ptr<TextInputHost> host);
    SessionId begin(const Request& request, Callback callback);
    void finish(SessionId id, bool accepted = true);
    void setText(SessionId id, const std::string& text);
    void setSelection(SessionId id, std::size_t anchor, std::size_t caret);
    void setPreedit(SessionId id, const std::string& text, std::size_t caret);
    void setComposing(SessionId id, bool composing);
    void setCaretRect(SessionId id, const sf::FloatRect& rect);
    const State* getState(SessionId id) const;
    bool isEditing() const;
    bool isModal() const;
    bool blocksGameplay() const;
    void beginFrame();
    void pump();
    bool processEvent(const sf::Event& event);
    void execute(Command command, bool extendSelection = false);
    void close();
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

LUDORK_ENGINE_API TextInputService& service();

}  // namespace ludork::engine::text_input
