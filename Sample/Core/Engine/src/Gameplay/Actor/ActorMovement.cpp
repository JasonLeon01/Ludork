#include <Gameplay/Actor.hpp>

#include <Gameplay/BPBase.hpp>
#include <Runtime/EngineState.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {

int directionComponent(int value) {
    return value > 0 ? 1 : value < 0 ? -1 : 0;
}

}  // namespace

void Actor::fixedUpdate(float fixedDelta) {
    const sf::Vector2f start = getPosition();
    float remaining = fixedDelta;
    while (remaining > 0.0f) {
        if (!moving_) {
            tryStartNextRouteStep();
        }
        if (!moving_) {
            const std::optional<sf::Vector2i> offset = _getContinueMoveOffset();
            if (offset.has_value()) {
                MapMove(*offset);
            }
        }
        if (!moving_) {
            break;
        }
        remaining = processMoving(remaining);
    }
    const float distance = (getPosition() - start).length();
    realSpeed_ =
        fixedDelta <= 0.0f || distance <= 0.001f ? 0.0f : distance / fixedDelta;
}

bool Actor::MapMove(const sf::Vector2i& requestedOffset) {
    const std::shared_ptr<ActorMapService> map = getMap();
    if (!moveEnabled_ || !map || moving_) {
        return false;
    }
    const sf::Vector2i offset(directionComponent(requestedOffset.x),
                              directionComponent(requestedOffset.y));
    if (offset.x == 0 && offset.y == 0) {
        return false;
    }
    const sf::Vector2i target = getMapPosition() + offset;
    const sf::Vector2u mapSize = map->getSize();
    if (target.x < 0 || target.y < 0 ||
        static_cast<unsigned int>(target.x) >= mapSize.x ||
        static_cast<unsigned int>(target.y) >= mapSize.y) {
        return false;
    }
    if (!map->isPassable(*this, target)) {
        const std::vector<Actor*> collisions = map->getCollision(*this, target);
        if (!collisions.empty()) {
            BPBase::BlueprintEventNative(
                *this, "onCollision", {{"other", actorListValue(collisions)}});
            if (isDestroyed()) {
                return false;
            }
            const std::vector<Actor*> self{this};
            for (Actor* collision : collisions) {
                if (collision == nullptr || collision->isDestroyed()) {
                    continue;
                }
                BPBase::BlueprintEventNative(*collision, "onCollision",
                                             {{"other", actorListValue(self)}});
                if (isDestroyed() || collision->isDestroyed()) {
                    break;
                }
            }
        }
        return false;
    }
    moving_ = true;
    moveOriginMapPosition_ = getMapPosition();
    departure_ = getPosition();
    destination_ =
        *departure_ + sf::Vector2f(static_cast<float>(offset.x * CellSize),
                                   static_cast<float>(offset.y * CellSize));
    return true;
}

bool Actor::isMoving() const {
    return moving_ || realSpeed_ > 0.0f || inRoute_;
}

sf::Vector2i Actor::getMapPosition() const {
    syncMapCache();
    if (moving_ && moveOriginMapPosition_.has_value()) {
        return *moveOriginMapPosition_;
    }
    return cachedMapPosition_;
}

bool Actor::isInRoute() const {
    return inRoute_;
}

void Actor::setRoute(const std::optional<std::vector<sf::Vector2i>>& route) {
    route_ = route;
    inRoute_ = route_.has_value() && !route_->empty();
}

std::optional<std::vector<sf::Vector2i>> Actor::getRoute() const {
    return route_;
}

bool Actor::getMoveEnabled() const {
    return moveEnabled_;
}

void Actor::setMoveEnabled(bool enabled) {
    moveEnabled_ = enabled;
    if (!moveEnabled_) {
        stop();
    }
}

void Actor::stop() {
    moving_ = false;
    inRoute_ = false;
    route_.reset();
    departure_.reset();
    destination_.reset();
    moveOriginMapPosition_.reset();
    realSpeed_ = 0.0f;
    autoFixMapPosition();
}

std::optional<sf::Vector2f> Actor::getVelocity() const {
    const std::shared_ptr<ActorMapService> map = getMap();
    if (!map || !departure_.has_value() || !destination_.has_value()) {
        return std::nullopt;
    }
    const std::optional<Material> topMaterial =
        map->getTopMaterial(getMapPosition());
    const float actualSpeed =
        speed * (topMaterial.has_value() ? topMaterial->speedRate : 1.0f);
    if (actualSpeed == 0.0f) {
        throw std::runtime_error("attempt to divide by zero");
    }
    const sf::Vector2f distance = *destination_ - *departure_;
    return distance / (distance.length() / actualSpeed);
}

std::optional<sf::Vector2i> Actor::_getContinueMoveOffset() {
    return std::nullopt;
}

void Actor::_onArrivedAtMapCell() {}

float Actor::processMoving(float deltaTime) {
    const std::optional<sf::Vector2f> velocity = getVelocity();
    const std::shared_ptr<ActorMapService> map = getMap();
    if (!velocity.has_value() || !destination_.has_value() ||
        !departure_.has_value() || !map) {
        return 0.0f;
    }
    const float remainingDistance = (*destination_ - getPosition()).length();
    const float actualSpeed = velocity->length();
    if (actualSpeed <= 0.0f) {
        return 0.0f;
    }
    const float time = remainingDistance / actualSpeed;
    if (time > deltaTime) {
        move(*velocity * deltaTime);
        return 0.0f;
    }
    setPosition(*destination_);
    moving_ = false;
    departure_.reset();
    destination_.reset();
    moveOriginMapPosition_.reset();
    autoFixMapPosition();
    const std::vector<Actor*> overlaps = map->getOverlaps(*this);
    if (!overlaps.empty()) {
        BPBase::BlueprintEventNative(*this, "onOverlap",
                                     {{"other", actorListValue(overlaps)}});
        const std::vector<Actor*> self{this};
        for (Actor* overlap : overlaps) {
            if (overlap != nullptr) {
                BPBase::BlueprintEventNative(*overlap, "onOverlap",
                                             {{"other", actorListValue(self)}});
            }
        }
    }
    _onArrivedAtMapCell();
    return std::max(0.0f, deltaTime - time);
}

void Actor::tryStartNextRouteStep() {
    if (!inRoute_) {
        return;
    }
    if (!route_.has_value() || route_->empty()) {
        inRoute_ = false;
        return;
    }
    const sf::Vector2i step = route_->front();
    route_->erase(route_->begin());
    if (!MapMove(step)) {
        inRoute_ = false;
        route_ = std::vector<sf::Vector2i>{};
    }
}

void Actor::autoFixMapPosition() {
    const sf::Vector2f position = getPosition();
    moveOriginMapPosition_.reset();
    setMapPosition(sf::Vector2u(
        static_cast<unsigned int>(position.x / static_cast<float>(CellSize) +
                                  0.5f),
        static_cast<unsigned int>(position.y / static_cast<float>(CellSize) +
                                  0.5f)));
    const std::shared_ptr<ActorMapService> map = getMap();
    if (map) {
        map->updateActorOccupancy(*this);
    }
}

RuntimeValue Actor::actorListValue(const std::vector<Actor*>& actors) {
    RuntimeValue::Array values;
    values.reserve(actors.size());
    for (Actor* actor : actors) {
        if (actor == nullptr) {
            continue;
        }
        std::shared_ptr<RuntimeObject> owner = actor->weak_from_this().lock();
        if (owner) {
            values.emplace_back(std::move(owner));
        }
    }
    return RuntimeValue(std::move(values));
}
