#include <Manager/TimeManager.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

sf::Clock TimeManager::clock_;
sf::Time TimeManager::lastElapsedTime_ = sf::Time::Zero;
sf::Time TimeManager::deltaTime_ = sf::Time::Zero;
float TimeManager::speed_ = 1.0f;
bool TimeManager::initialized_ = false;

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
    clock_.restart();
    lastElapsedTime_ = sf::Time::Zero;
    deltaTime_ = sf::Time::Zero;
    initialized_ = true;
    update();
}

sf::Time TimeManager::getCurrentTime() {
    if (!initialized_) {
        init();
    }
    return lastElapsedTime_;
}

sf::Time TimeManager::getDeltaTime() {
    if (!initialized_) {
        init();
    }
    return sf::seconds(deltaTime_.asSeconds() * speed_);
}

void TimeManager::update() {
    if (!initialized_) {
        init();
        return;
    }
    const sf::Time currentTime = clock_.getElapsedTime();
    deltaTime_ = currentTime - lastElapsedTime_;
    lastElapsedTime_ = currentTime;
}

float TimeManager::getSpeed() {
    return speed_;
}

void TimeManager::setSpeed(float speed) {
    if (speed < 0.0f) {
        throw std::invalid_argument("Time speed must be non-negative");
    }
    speed_ = speed;
}

void TimeManager::shutdown() noexcept {
    lastElapsedTime_ = sf::Time::Zero;
    deltaTime_ = sf::Time::Zero;
    speed_ = 1.0f;
    initialized_ = false;
}
