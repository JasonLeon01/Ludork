#include <Emitters/EmitterScheduler.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

void EmitterScheduler::beginFrame() {
    active_.clear();
    previous_.swap(registered_);
    registered_.clear();
    Emitter::collectGarbage();
    std::erase_if(known_, [](const auto& entry) {
        return entry.second.expired();
    });
}

void EmitterScheduler::registerEmitter(const std::shared_ptr<Emitter>& emitter,
                                       const sf::Transform& hostTransform) {
    if (emitter == nullptr) {
        return;
    }
    if (!registered_.insert(emitter.get()).second) {
        return;
    }
    if (!previous_.contains(emitter.get())) {
        emitter->resetHostMotion();
    }
    emitter->setHostTransform(hostTransform);
    known_.insert_or_assign(emitter.get(), emitter);
    active_.push_back(emitter);
}

void EmitterScheduler::advance(float deltaTime) {
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f) {
        throw std::invalid_argument(
            "Emitter delta time must be finite and non-negative");
    }
    for (const std::weak_ptr<Emitter>& entry : active_) {
        const std::shared_ptr<Emitter> emitter = entry.lock();
        if (emitter != nullptr && known_.contains(emitter.get())) {
            emitter->tick(deltaTime);
        }
    }
}

void EmitterScheduler::shutdown() noexcept {
    for (const auto& [_, entry] : known_) {
        if (const std::shared_ptr<Emitter> emitter = entry.lock()) {
            emitter->stop(true);
            emitter->shutdown();
        }
    }
    known_.clear();
    active_.clear();
    registered_.clear();
    previous_.clear();
    Emitter::collectGarbage();
}
