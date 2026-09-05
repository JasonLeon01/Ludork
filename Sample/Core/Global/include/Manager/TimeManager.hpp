#pragma once

#include <CoreMinimal.hpp>

#include <atomic>
#include <mutex>

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
    static void init();

    BIND_METHOD()
    static sf::Time getCurrentTime();

    BIND_METHOD()
    static sf::Time getDeltaTime();

    static void update();

    BIND_METHOD()
    static float getSpeed();

    BIND_METHOD()
    static void setSpeed(float speed);

    static void shutdown() noexcept;

private:
    static void ensureInitialized();
    static void initializeLocked();

    static sf::Clock clock_;
    static sf::Time writerLastElapsedTime_;
    static std::atomic<std::int64_t> currentMicroseconds_;
    static std::atomic<std::int64_t> deltaMicroseconds_;
    static std::atomic<float> speed_;
    static std::atomic_bool initialized_;
    static std::mutex writerMutex_;
};
