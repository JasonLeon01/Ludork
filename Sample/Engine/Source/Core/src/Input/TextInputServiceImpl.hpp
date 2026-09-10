#pragma once

#include <Input/TextInputService.hpp>
#include <deque>
#include <mutex>
#include <utility>
#include <set>

namespace ludork::engine::text_input {

struct TextInputService::Impl {
    struct Queue {
        std::mutex mutex;
        std::deque<std::pair<SessionId, Event>> events;
    };
    std::shared_ptr<Queue> queue = std::make_shared<Queue>();
    std::shared_ptr<TextInputHost> host;
    SessionId nextId = 0;
    SessionId id = 0;
    State state;
    State original;
    sf::FloatRect caretRect;
    Callback callback;
    bool blocked = false;
    bool compositionInFrame = false;
    bool composing = false;
    unsigned int releaseFrames = 0;
    std::set<sf::Keyboard::Key> heldKeys;
    std::set<sf::Keyboard::Key> activationKeys;
    std::set<sf::Keyboard::Key> releaseKeys;

    void notify();
    void syncHost();
    void normalize();
    void insert(const std::string& value);
};

}  // namespace ludork::engine::text_input
