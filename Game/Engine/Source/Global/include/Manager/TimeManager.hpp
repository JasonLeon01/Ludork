#pragma once

#include <CoreMinimal.hpp>

#include <atomic>
#include <mutex>

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
