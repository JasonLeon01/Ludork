#include <Manager/TimeManager.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

sf::Clock TimeManager::clock_;
sf::Time TimeManager::writerLastElapsedTime_ = sf::Time::Zero;
std::atomic<std::int64_t> TimeManager::currentMicroseconds_ = 0;
std::atomic<std::int64_t> TimeManager::deltaMicroseconds_ = 0;
std::atomic<float> TimeManager::speed_ = 1.0f;
std::atomic_bool TimeManager::initialized_ = false;
std::mutex TimeManager::writerMutex_;

TimerEntry::TimerEntry(float timeValue, RuntimeIdentityPtr taskValue,
                       RuntimeValue::Array paramsValue, bool blockingValue)
    : time(timeValue),
      task(std::move(taskValue)),
      params(std::move(paramsValue)),
      blocking(blockingValue) {}

bool TimerEntry::isReady() const {
    return time <= 0.0f;
}

bool TimerEntry::isCancelled() const {
    return cancelled_;
}

void TimerEntry::cancel() {
    cancelled_ = true;
    time = 0.0f;
}

void TimeManager::init() {
    const std::lock_guard<std::mutex> lock(writerMutex_);
    initializeLocked();
}

void TimeManager::ensureInitialized() {
    if (initialized_.load(std::memory_order_acquire)) {
        return;
    }
    const std::lock_guard<std::mutex> lock(writerMutex_);
    if (!initialized_.load(std::memory_order_relaxed)) {
        initializeLocked();
    }
}

void TimeManager::initializeLocked() {
    initialized_.store(false, std::memory_order_release);
    clock_.restart();
    writerLastElapsedTime_ = clock_.getElapsedTime();
    currentMicroseconds_.store(writerLastElapsedTime_.asMicroseconds(),
                               std::memory_order_release);
    deltaMicroseconds_.store(writerLastElapsedTime_.asMicroseconds(),
                             std::memory_order_release);
    initialized_.store(true, std::memory_order_release);
}

sf::Time TimeManager::getCurrentTime() {
    ensureInitialized();
    return sf::microseconds(
        currentMicroseconds_.load(std::memory_order_acquire));
}

sf::Time TimeManager::getDeltaTime() {
    ensureInitialized();
    const sf::Time delta = sf::microseconds(
        deltaMicroseconds_.load(std::memory_order_acquire));
    return delta * speed_.load(std::memory_order_acquire);
}

void TimeManager::update() {
    const std::lock_guard<std::mutex> lock(writerMutex_);
    if (!initialized_.load(std::memory_order_relaxed)) {
        initializeLocked();
        return;
    }
    const sf::Time currentTime = clock_.getElapsedTime();
    const sf::Time deltaTime = currentTime - writerLastElapsedTime_;
    writerLastElapsedTime_ = currentTime;
    deltaMicroseconds_.store(deltaTime.asMicroseconds(),
                             std::memory_order_release);
    currentMicroseconds_.store(currentTime.asMicroseconds(),
                               std::memory_order_release);
}

float TimeManager::getSpeed() {
    return speed_.load(std::memory_order_acquire);
}

void TimeManager::setSpeed(float speed) {
    if (speed < 0.0f) {
        throw std::invalid_argument("Time speed must be non-negative");
    }
    speed_.store(speed, std::memory_order_release);
}

void TimeManager::shutdown() noexcept {
    const std::lock_guard<std::mutex> lock(writerMutex_);
    initialized_.store(false, std::memory_order_release);
    writerLastElapsedTime_ = sf::Time::Zero;
    currentMicroseconds_.store(0, std::memory_order_release);
    deltaMicroseconds_.store(0, std::memory_order_release);
    speed_.store(1.0f, std::memory_order_release);
}
