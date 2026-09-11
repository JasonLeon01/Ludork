#include <Gameplay/Components/EmitterComponent.hpp>

#include <Emitters/EmitterScheduler.hpp>
#include <Gameplay/Actor.hpp>
#include <Runtime/RuntimeReference.hpp>

#include <cmath>
#include <stdexcept>

EmitterComponent::~EmitterComponent() {
    release();
}

RuntimeValue::Array EmitterComponent::onAttach(
    const RuntimeIdentityPtr& owner) {
    const std::shared_ptr<Actor> actor = std::dynamic_pointer_cast<Actor>(
        ludork::runtime::reference::object(RuntimeValue(owner)));
    if (actor == nullptr) {
        throw std::invalid_argument("EmitterComponent owner must be an Actor");
    }
    if (const std::shared_ptr<Actor> previous = owner_.lock();
        previous != nullptr && previous != actor) {
        release();
    }
    owner_ = ludork::runtime::detail::canonicalRuntimeOwner(actor);
    return {};
}

std::shared_ptr<Emitter> EmitterComponent::getEmitter() {
    if (emitter_ == nullptr || loadedResource_ != resource) {
        release();
        emitter_ = std::make_shared<Emitter>(resource);
        loadedResource_ = resource;
        if (!resource.empty()) {
            emitter_->play();
        }
    }
    return emitter_;
}

void EmitterComponent::collect(Actor& owner, EmitterScheduler& scheduler) {
    if (resource.empty()) {
        release();
        return;
    }
    if (owner.isDestroyed() || !owner.isVisibleInHierarchy() ||
        owner_.lock().get() != &owner) {
        return;
    }
    if (!std::isfinite(anchor.x) || !std::isfinite(anchor.y) ||
        !std::isfinite(offset.x) || !std::isfinite(offset.y) ||
        !std::isfinite(rotation) || !std::isfinite(scale.x) ||
        !std::isfinite(scale.y)) {
        throw std::invalid_argument(
            "EmitterComponent transform must be finite");
    }
    const sf::FloatRect bounds = owner.getLocalBounds();
    sf::Transform transform = owner.getTransform();
    transform.translate(bounds.position +
                        sf::Vector2f{bounds.size.x * anchor.x + offset.x,
                                     bounds.size.y * anchor.y + offset.y});
    transform.rotate(sf::degrees(rotation));
    transform.scale(scale);
    scheduler.registerEmitter(getEmitter(), transform);
}

void EmitterComponent::draw(Actor& owner, sf::RenderTarget& target,
                            sf::RenderStates states, bool before) {
    if (beforeActor != before || emitter_ == nullptr || resource.empty() ||
        loadedResource_ != resource || owner.isDestroyed() ||
        !owner.isVisibleInHierarchy() || owner_.lock().get() != &owner) {
        return;
    }
    emitter_->draw(target, states);
}

void EmitterComponent::release() noexcept {
    if (emitter_ != nullptr) {
        emitter_->stop(true);
        emitter_->shutdown();
        emitter_.reset();
    }
    loadedResource_.clear();
}
