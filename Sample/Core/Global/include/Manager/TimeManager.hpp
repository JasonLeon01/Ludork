#pragma once

#include <BindAnnotations.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <SFML/System.hpp>

#include <functional>
#include <memory>

BIND_CLASS()
class TimerEntry {
public:
    BIND_INIT(defaults = {nil, {}, false})
    TimerEntry(float time, RuntimeIdentityPtr task = {},
               RuntimeValue::Array params = {}, bool blocking = false);

    BIND_PROPERTY()
    float time;

    BIND_PROPERTY(metadata_type = "function")
    RuntimeIdentityPtr task;

    BIND_PROPERTY()
    RuntimeValue::Array params;

    BIND_PROPERTY()
    bool blocking;

    BIND_METHOD()
    bool isReady() const;

    BIND_METHOD(Pure = true)
    bool isCancelled() const;

    BIND_METHOD()
    void cancel();

private:
    bool cancelled_ = false;
};

using TimerHandle = std::function<bool()>;

BIND_CLASS()
class TimeManager {
public:
    BIND_IGNORE()
    static void init();

    BIND_METHOD()
    static sf::Time getCurrentTime();

    BIND_METHOD()
    static sf::Time getDeltaTime();

    BIND_IGNORE()
    static void update();

    BIND_METHOD()
    static float getSpeed();

    BIND_METHOD()
    static void setSpeed(float speed);

    BIND_IGNORE()
    static void shutdown() noexcept;

private:
    static sf::Clock clock_;
    static sf::Time lastElapsedTime_;
    static sf::Time deltaTime_;
    static float speed_;
    static bool initialized_;
};
