#include <Gameplay/Actor.hpp>

#include "Actor/AudioService.hpp"

#include <Gameplay/BPBase.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <stdexcept>
#include <utility>

namespace {

RuntimeValue optionalStringValue(const std::optional<std::string>& value) {
    return value.has_value() ? RuntimeValue(*value) : RuntimeValue();
}

}  // namespace

Actor::Actor(std::shared_ptr<sf::Texture> texture,
             std::optional<sf::IntRect> rect, std::string actorTag)
    : sf::Sprite(textureOrBlank(texture)),
      tag(actorTag),
      texture_(std::move(texture)),
      spriteTexture_(texture_),
      mapTag_(actorTag) {
    if (rect.has_value()) {
        sf::Sprite::setTextureRect(*rect);
    }
    ensureShaderLoaded();
    syncMapCache();
}

bool Actor::getCollisionEnabled() const {
    return collisionEnabled;
}

void Actor::setCollisionEnabled(bool enabled) {
    collisionEnabled = enabled;
}

bool Actor::getPathfindingBlocks() const {
    return pathfindingBlocks_;
}

void Actor::setPathfindingBlocks(bool blocks) {
    pathfindingBlocks_ = blocks;
}

bool Actor::isDestroyed() const {
    return destroyed_;
}

void Actor::markDestroyed(bool destroyed) {
    destroyed_ = destroyed;
}

void Actor::lateUpdate(float deltaTime) {
    static_cast<void>(deltaTime);
}

void Actor::setGraph(const RuntimeIdentityPtr& graph) {
    graph_ = graph;
}

RuntimeIdentityPtr Actor::getGraph() const {
    return graph_;
}

bool Actor::hasGraph() const {
    return static_cast<bool>(graph_);
}

const std::string& Actor::getMapTag() const {
    return mapTag_;
}

void Actor::setMapTag(const std::string& value) {
    mapTag_ = value;
}

void Actor::ensureMapTag() {
    bool generatedClass = false;
    const std::shared_ptr<RuntimeObject> self = weak_from_this().lock();
    if (self != nullptr) {
        const std::vector<RuntimeValue> types =
            resolveRuntime("reflect.type", {RuntimeValue(self)});
        if (!types.empty()) {
            const std::vector<RuntimeValue> values = resolveRuntime(
                "reflect.get",
                {types.front(), RuntimeValue(std::string("_GENERATED_CLASS"))});
            if (!values.empty()) {
                const bool* flag = values.front().getIf<bool>();
                generatedClass = flag != nullptr && *flag;
            }
        }
    }
    if (mapTag_.empty() && !generatedClass) {
        mapTag_ = tag;
    }
}

void Actor::update(float deltaTime) {
    ensureShaderLoaded();
    if (animatable) {
        _animate(deltaTime);
    }
    updateAutoSound(deltaTime);
}

void Actor::onCreate() {}

void Actor::onTick(float deltaTime) {
    static_cast<void>(deltaTime);
}

void Actor::onLateTick(float deltaTime) {
    static_cast<void>(deltaTime);
}

void Actor::onFixedTick(float fixedDelta) {
    static_cast<void>(fixedDelta);
}

void Actor::onDestroy() {}

void Actor::onWorldSleep() {}

void Actor::onWorldWake(float elapsedSeconds) {
    static_cast<void>(elapsedSeconds);
}

void Actor::onCollision(const std::vector<Actor*>& other) {
    static_cast<void>(other);
}

void Actor::onOverlap(const std::vector<Actor*>& other) {
    static_cast<void>(other);
}

void Actor::destroy() {
    if (isDestroyed()) {
        return;
    }
    BPBase::BlueprintEventNative(*this, "onDestroy");
    markDestroyed(true);
    stopAutoSound();
    const std::vector<std::shared_ptr<Actor>> children = getChildren();
    for (const std::shared_ptr<Actor>& child : children) {
        if (child) {
            child->destroy();
        }
    }
    const std::shared_ptr<ActorMapService> map = getMap();
    if (map) {
        map->destroyActor(*this);
    }
}

bool Actor::getTickable() const {
    return tickable;
}

void Actor::setTickable(bool tickable, bool applyToChildren) {
    this->tickable = tickable;
    if (!applyToChildren) {
        return;
    }
    for (const std::shared_ptr<Actor>& child : getChildren()) {
        if (child) {
            child->setTickable(tickable, true);
        }
    }
}

void Actor::BlueprintEvent(const RuntimeIdentityPtr& object,
                           const RuntimeIdentityPtr& objectType,
                           const std::string& eventName,
                           const RuntimeValue& keywordArguments,
                           const RuntimeIdentityPtr& onComplete) {
    BPBase::BlueprintEvent(object, objectType, eventName, keywordArguments,
                           onComplete);
}

bool Actor::HasBlueprintEvent(const RuntimeIdentityPtr& object,
                              const std::string& eventName) {
    return BPBase::HasBlueprintEvent(object, eventName);
}

bool Actor::IsBlueprintEventEmpty(const RuntimeIdentityPtr& object,
                                  const std::string& eventName) {
    return BPBase::IsBlueprintEventEmpty(object, eventName);
}

RuntimeValue Actor::GenActor(const RuntimeIdentityPtr& actorModel,
                             const RuntimeIdentityPtr& texture,
                             const RuntimeValue& textureRect,
                             const std::optional<std::string>& tag) {
    std::vector<RuntimeValue> result = resolveRuntime(
        "class.construct", {RuntimeValue(actorModel), RuntimeValue(texture),
                            textureRect, optionalStringValue(tag)});
    if (result.empty() || result.front().isNil()) {
        throw std::runtime_error(
            "Actor runtime construction returned no instance");
    }
    return std::move(result.front());
}

std::shared_ptr<AutoSoundParams> Actor::getAutoSoundParams() const {
    return autoSoundParams_;
}

void Actor::setAutoSoundParams(const AutoSoundParams& params) {
    autoSoundParams_ = std::make_shared<AutoSoundParams>(params);
}

void Actor::normaliseAutoSoundParams() {
    autoSoundParams_ =
        autoSoundParams_ ? std::make_shared<AutoSoundParams>(*autoSoundParams_)
                         : std::make_shared<AutoSoundParams>();
}

float Actor::autoSoundListenerDistance() const {
    const sf::Vector3f listenerPosition = sf::Listener::getPosition();
    const sf::Vector2f actorPosition = getPosition();
    const float x = actorPosition.x - listenerPosition.x;
    const float y = actorPosition.y - listenerPosition.y;
    return std::sqrt(x * x + y * y);
}

void Actor::updateAutoSound(float deltaTime) {
    normaliseAutoSoundParams();
    if (autoSound.empty()) {
        stopAutoSound();
        autoSoundCooldown_ = 0.0f;
        return;
    }
    const float stopDistance = autoSoundParams_->maxDistance;
    if (stopDistance > 0.0f) {
        const float distance = autoSoundListenerDistance();
        const float startDistance = stopDistance * 0.85f;
        if (distance > stopDistance) {
            stopAutoSound();
            autoSoundCooldown_ = 0.0f;
            return;
        }
        if (!autoSoundObject_ && distance > startDistance) {
            return;
        }
    }
    if (autoSoundObject_) {
        if (autoSoundObject_->getStatus() == sf::SoundSource::Status::Stopped) {
            autoSoundObject_.reset();
            autoSoundCooldown_ = std::max(0.0f, autoSoundInterval);
        } else {
            applyAutoSoundParams();
            return;
        }
    }
    if (autoSoundCooldown_ > 0.0f) {
        autoSoundCooldown_ = std::max(0.0f, autoSoundCooldown_ - deltaTime);
        return;
    }
    playAutoSound();
}

void Actor::playAutoSound() {
    if (actorAudioService() == nullptr) {
        return;
    }
    autoSoundObject_ =
        actorAudioService()->playSoundEffect(autoSound, buildAutoSoundFilter());
    if (!autoSoundObject_) {
        return;
    }
    const sf::Vector2f position = getPosition();
    autoSoundLastPosition_ = sf::Vector3f(position.x, position.y, 0.0f);
}

void Actor::stopAutoSound() {
    if (!autoSoundObject_) {
        return;
    }
    if (autoSoundObject_->getStatus() != sf::SoundSource::Status::Stopped) {
        autoSoundObject_->stop();
    }
    autoSoundObject_.reset();
    autoSoundLastPosition_.reset();
}

void Actor::applyAutoSoundParams() {
    if (!autoSoundObject_ || actorAudioService() == nullptr) {
        return;
    }
    const sf::Vector2f position = getPosition();
    const sf::Vector3f newPosition(position.x, position.y, 0.0f);
    if (autoSoundLastPosition_.has_value() &&
        *autoSoundLastPosition_ == newPosition) {
        return;
    }
    autoSoundLastPosition_ = newPosition;
    actorAudioService()->setSoundFilter(autoSoundObject_,
                                        buildAutoSoundFilter());
}

SoundFilter Actor::buildAutoSoundFilter() const {
    SoundFilter filter;
    const sf::Vector2f position = getPosition();
    filter.volume = autoSoundParams_->volume;
    filter.spatial = true;
    filter.position = sf::Vector3f(position.x, position.y, 0.0f);
    filter.relativeToListener = false;
    filter.minDistance = autoSoundParams_->minDistance;
    filter.attenuation = autoSoundParams_->attenuation;
    if (autoSoundParams_->loop) {
        filter.loop = true;
    }
    if (autoSoundParams_->maxDistance > 0.0f) {
        filter.maxDistance = autoSoundParams_->maxDistance;
    }
    return filter;
}

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

#include <Gameplay/Actor.hpp>

#include <Runtime/EngineState.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

bool boundsIntersectCell(const sf::Vector2f& boundsPos,
                         const sf::Vector2f& boundsSize, float cellX,
                         float cellY, float cellSize) {
    const float boundsRight = boundsPos.x + boundsSize.x;
    const float boundsBottom = boundsPos.y + boundsSize.y;
    const float cellRight = cellX + cellSize;
    const float cellBottom = cellY + cellSize;
    return boundsPos.x < cellRight && boundsRight > cellX &&
           boundsPos.y < cellBottom && boundsBottom > cellY;
}

int roundHalfToEven(float value) {
    const float lower = std::floor(value);
    const float fraction = value - lower;
    if (fraction < 0.5f) {
        return static_cast<int>(lower);
    }
    if (fraction > 0.5f) {
        return static_cast<int>(lower + 1.0f);
    }
    const int lowerInteger = static_cast<int>(lower);
    return lowerInteger % 2 == 0 ? lowerInteger : lowerInteger + 1;
}

}  // namespace

ActorMapService::~ActorMapService() = default;

sf::Vector2f Actor::getPosition() const {
    return sf::Sprite::getPosition() - translation_;
}

sf::Vector2f Actor::getRelativePosition() const {
    return relativePosition_;
}

std::vector<sf::Vector2i> Actor::getOccupiedMapCells(
    const std::optional<sf::Vector2f>& worldPosition) const {
    syncMapCache();
    if (!worldPosition.has_value()) {
        return occupiedCells_;
    }
    const sf::Vector2f delta = *worldPosition - getPosition();
    const sf::Vector2i mapDelta(
        roundHalfToEven(delta.x / static_cast<float>(CellSize)),
        roundHalfToEven(delta.y / static_cast<float>(CellSize)));
    return getOccupiedMapCellsAtMapPosition(getMapPosition() + mapDelta);
}

std::vector<sf::Vector2i> Actor::getOccupiedMapCellsAtMapPosition(
    const sf::Vector2i& mapPosition) const {
    syncMapCache();
    const sf::Vector2i delta = mapPosition - cachedMapPosition_;
    const sf::Vector2f worldPosition(
        cachedPosition_.x + static_cast<float>(delta.x * CellSize),
        cachedPosition_.y + static_cast<float>(delta.y * CellSize));
    sf::FloatRect bounds = cachedGlobalBounds_;
    bounds.position += worldPosition - cachedPosition_;
    return computeOccupiedCells(bounds);
}

sf::Vector2i Actor::getRelativeMapPosition() const {
    return {
        static_cast<int>(relativePosition_.x / static_cast<float>(CellSize)),
        static_cast<int>(relativePosition_.y / static_cast<float>(CellSize))};
}

sf::FloatRect Actor::getLocalBounds() const {
    return sf::Sprite::getLocalBounds();
}

sf::FloatRect Actor::getGlobalBounds() const {
    return sf::Sprite::getGlobalBounds();
}

void Actor::setPosition(const sf::Vector2f& position) {
    const std::shared_ptr<Actor> parent = getParent();
    if (parent != nullptr) {
        relativePosition_ = position - parent->getPosition();
    } else {
        relativePosition_ = {};
    }
    sf::Sprite::setPosition(position + translation_);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updatePositionFromParent();
    }
    syncMapCache();
}

void Actor::setRelativePosition(const sf::Vector2f& position) {
    const std::shared_ptr<Actor> parent = getParent();
    const sf::Vector2f parentPosition =
        parent == nullptr ? sf::Vector2f{} : parent->getPosition();
    setPosition(parentPosition + position);
}

void Actor::setMapPosition(const sf::Vector2u& position) {
    setPosition({static_cast<float>(position.x * CellSize),
                 static_cast<float>(position.y * CellSize)});
}

void Actor::setMapPosition(const sf::Vector2i& position) {
    if (position.x < 0 || position.y < 0) {
        throw std::invalid_argument("Map position must be non-negative");
    }
    setMapPosition(sf::Vector2u(static_cast<unsigned int>(position.x),
                                static_cast<unsigned int>(position.y)));
}

void Actor::setMapPositionSigned(const sf::Vector2i& position) {
    setMapPosition(position);
}

void Actor::setRelativeMapPosition(const sf::Vector2u& position) {
    setRelativePosition({static_cast<float>(position.x * CellSize),
                         static_cast<float>(position.y * CellSize)});
}

void Actor::move(const sf::Vector2f& offset) {
    sf::Sprite::move(offset);
    relativePosition_ += offset;
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updatePositionFromParent();
    }
    syncMapCache();
}

sf::Angle Actor::getRotation() const {
    return sf::Sprite::getRotation();
}

sf::Angle Actor::getRelativeRotation() const {
    return relativeRotation_;
}

void Actor::setRotation(sf::Angle angle) {
    const std::shared_ptr<Actor> parent = getParent();
    relativeRotation_ = parent == nullptr
                            ? sf::Angle::Zero
                            : sf::degrees(angle.asDegrees() -
                                          parent->getRotation().asDegrees());
    sf::Sprite::setRotation(angle);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateRotationFromParent();
    }
    syncMapCache();
}

void Actor::setRotationDegrees(float angle) {
    setRotation(sf::degrees(angle));
}

void Actor::rotate(sf::Angle angle) {
    relativeRotation_ =
        sf::degrees(relativeRotation_.asDegrees() + angle.asDegrees());
    sf::Sprite::rotate(angle);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateRotationFromParent();
    }
    syncMapCache();
}

void Actor::rotateDegrees(float angle) {
    rotate(sf::degrees(angle));
}

void Actor::setRelativeRotation(sf::Angle angle) {
    const std::shared_ptr<Actor> parent = getParent();
    const float parentDegrees =
        parent == nullptr ? 0.0f : parent->getRotation().asDegrees();
    setRotation(sf::degrees(parentDegrees + angle.asDegrees()));
}

void Actor::setRelativeRotationDegrees(float angle) {
    setRelativeRotation(sf::degrees(angle));
}

sf::Vector2f Actor::getScale() const {
    return sf::Sprite::getScale();
}

sf::Vector2f Actor::getRelativeScale() const {
    return relativeScale_;
}

void Actor::setScale(const sf::Vector2f& factors) {
    const std::shared_ptr<Actor> parent = getParent();
    relativeScale_ = parent == nullptr
                         ? sf::Vector2f{1.0f, 1.0f}
                         : factors.componentWiseDiv(parent->getScale());
    sf::Sprite::setScale(factors);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateScaleFromParent();
    }
    syncMapCache();
}

void Actor::scale(const sf::Vector2f& factor) {
    relativeScale_ = relativeScale_.componentWiseMul(factor);
    sf::Sprite::scale(factor);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateScaleFromParent();
    }
    syncMapCache();
}

void Actor::setRelativeScale(const sf::Vector2f& scale) {
    const std::shared_ptr<Actor> parent = getParent();
    const sf::Vector2f parentScale =
        parent == nullptr ? sf::Vector2f{1.0f, 1.0f} : parent->getScale();
    setScale(parentScale.componentWiseMul(scale));
}

sf::Vector2f Actor::getOrigin() const {
    return sf::Sprite::getOrigin();
}

void Actor::setOrigin(const sf::Vector2f& origin) {
    sf::Sprite::setOrigin(origin);
    syncMapCache();
}

sf::Vector2f Actor::getTranslation() const {
    return translation_;
}

void Actor::setTranslation(const sf::Vector2f& translation) {
    const sf::Vector2f position = getPosition();
    translation_ = translation;
    setPosition(position);
}

void Actor::setAlignment(const sf::Vector2f& alignment) {
    const sf::IntRect rect = sf::Sprite::getTextureRect();
    const float x = std::clamp(alignment.x, 0.0f, 1.0f);
    const float y = std::clamp(alignment.y, 0.0f, 1.0f);
    setOrigin({static_cast<float>(rect.size.x) * x,
               static_cast<float>(rect.size.y) * y});
}

std::shared_ptr<ActorMapService> Actor::getMap() const {
    return map_.lock();
}

void Actor::setMap(const std::shared_ptr<ActorMapService>& inMap) {
    map_ = ludork::engine::runtime_detail::canonicalRuntimeOwner(inMap);
}

std::shared_ptr<Actor> Actor::getParent() const {
    return parent_.lock();
}

void Actor::setParent(const std::shared_ptr<Actor>& parent) {
    parent_ = ludork::engine::runtime_detail::canonicalRuntimeOwner(parent);
}

const std::vector<std::shared_ptr<Actor>>& Actor::getChildren() const {
    return children_;
}

std::vector<std::shared_ptr<Actor>> Actor::collectTree() {
    const std::shared_ptr<Actor> root =
        std::static_pointer_cast<Actor>(weak_from_this().lock());
    if (root == nullptr) {
        throw std::logic_error("Actor hierarchy root is not shared");
    }
    std::vector<std::shared_ptr<Actor>> result{root};
    std::unordered_set<Actor*> visited{this};
    for (std::size_t index = 0; index < result.size(); ++index) {
        for (const std::shared_ptr<Actor>& child : result[index]->children_) {
            if (child == nullptr) {
                throw std::logic_error("Actor hierarchy contains a null child");
            }
            if (!visited.insert(child.get()).second) {
                throw std::logic_error(
                    "Actor hierarchy contains a cycle or repeated Actor");
            }
            result.push_back(child);
        }
    }
    return result;
}

void Actor::addChild(const std::shared_ptr<Actor>& child) {
    if (!child || child.get() == this) {
        return;
    }
    const auto iterator = std::find(children_.begin(), children_.end(), child);
    if (iterator != children_.end()) {
        std::cerr << "Child already exists\n";
        return;
    }
    children_.push_back(child);
    const std::shared_ptr<Actor> self =
        std::static_pointer_cast<Actor>(weak_from_this().lock());
    if (self == nullptr) {
        throw std::logic_error("Actor hierarchy owner is not shared");
    }
    child->setParent(self);
    if (const std::shared_ptr<ActorMapService> map = getMap()) {
        child->setMap(map);
        map->updateActorList();
    }
}

void Actor::removeChild(const std::shared_ptr<Actor>& child) {
    const auto iterator = std::find(children_.begin(), children_.end(), child);
    if (iterator == children_.end()) {
        throw std::invalid_argument("Child not found");
    }
    children_.erase(iterator);
    child->setParent(nullptr);
}

void Actor::syncMapCache() const {
    Actor* self = const_cast<Actor*>(this);
    self->cachedPosition_ = getPosition();
    self->cachedGlobalBounds_ = sf::Sprite::getGlobalBounds();
    self->cachedMapPosition_ = sf::Vector2i(
        static_cast<int>(
            self->cachedPosition_.x / static_cast<float>(CellSize) + 0.5f),
        static_cast<int>(
            self->cachedPosition_.y / static_cast<float>(CellSize) + 0.5f));
    self->occupiedCells_ = computeOccupiedCells(self->cachedGlobalBounds_);
}

void Actor::_superMove(const sf::Vector2f& offset) {
    sf::Sprite::move(offset);
}

void Actor::_updatePositionFromParent() {
    const std::shared_ptr<Actor> parent = getParent();
    if (parent == nullptr) {
        return;
    }
    sf::Sprite::setPosition(parent->getPosition() + relativePosition_);
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updatePositionFromParent();
    }
}

void Actor::_updateRotationFromParent() {
    const std::shared_ptr<Actor> parent = getParent();
    if (parent == nullptr) {
        return;
    }
    sf::Sprite::setRotation(sf::degrees(parent->getRotation().asDegrees() +
                                        relativeRotation_.asDegrees()));
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateRotationFromParent();
    }
}

void Actor::_updateScaleFromParent() {
    const std::shared_ptr<Actor> parent = getParent();
    if (parent == nullptr) {
        return;
    }
    sf::Sprite::setScale(parent->getScale().componentWiseMul(relativeScale_));
    for (const std::shared_ptr<Actor>& child : children_) {
        child->_updateScaleFromParent();
    }
}

bool Actor::intersects(const Actor& other) const {
    return getGlobalBounds()
        .findIntersection(other.getGlobalBounds())
        .has_value();
}

void Actor::refreshDescendantCache() {
    descendantActors_.clear();
    std::vector<Actor*> stack;
    stack.reserve(children_.size());
    for (const std::shared_ptr<Actor>& child : children_) {
        if (child != nullptr) {
            stack.push_back(child.get());
        }
    }
    while (!stack.empty()) {
        Actor* child = stack.back();
        stack.pop_back();
        if (!descendantActors_.insert(child).second) {
            continue;
        }
        for (const std::shared_ptr<Actor>& nextChild : child->children_) {
            if (nextChild != nullptr) {
                stack.push_back(nextChild.get());
            }
        }
    }
}

const std::unordered_set<Actor*>& Actor::getDescendantActors() const {
    return descendantActors_;
}

bool Actor::blocksPassability() const {
    return getCollisionEnabled() || getPathfindingBlocks();
}

std::vector<sf::Vector2i> Actor::computeOccupiedCells(
    const sf::FloatRect& bounds) const {
    if (bounds.size.x <= 0.f || bounds.size.y <= 0.f) {
        return {cachedMapPosition_};
    }
    const float cellSize = static_cast<float>(CellSize);
    const int minX = static_cast<int>(std::floor(bounds.position.x / cellSize));
    const int minY = static_cast<int>(std::floor(bounds.position.y / cellSize));
    const int maxX = static_cast<int>(
        std::floor((bounds.position.x + bounds.size.x - 1e-9f) / cellSize));
    const int maxY = static_cast<int>(
        std::floor((bounds.position.y + bounds.size.y - 1e-9f) / cellSize));
    std::vector<sf::Vector2i> cells;
    for (int cellY = minY; cellY <= maxY; ++cellY) {
        for (int cellX = minX; cellX <= maxX; ++cellX) {
            if (boundsIntersectCell(bounds.position, bounds.size,
                                    static_cast<float>(cellX) * cellSize,
                                    static_cast<float>(cellY) * cellSize,
                                    cellSize)) {
                cells.emplace_back(cellX, cellY);
            }
        }
    }
    if (cells.empty()) {
        return {cachedMapPosition_};
    }
    return cells;
}

#include <Gameplay/Actor.hpp>

#include <ConcurrentResourceCache.hpp>
#include <Utils/ShaderLoader.hpp>

#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

std::unique_ptr<sf::Texture>& blankTextureStorage() {
    static std::unique_ptr<sf::Texture> texture;
    return texture;
}

ludork::core::ConcurrentResourceCache<sf::Shader, true>& actorShaderCache() {
    static ludork::core::ConcurrentResourceCache<sf::Shader, true> cache;
    return cache;
}

const sf::Texture& blankTexture() {
    std::unique_ptr<sf::Texture>& texture = blankTextureStorage();
    if (texture == nullptr) {
        texture = std::make_unique<sf::Texture>();
    }
    return *texture;
}

}  // namespace

const sf::Texture& Actor::textureOrBlank(
    const std::shared_ptr<sf::Texture>& texture) {
    return texture ? *texture : blankTexture();
}

void Actor::ensureShaderLoaded() const {
    if (loadedShaderPath_ == shaderPath) {
        return;
    }
    loadedShaderPath_ = shaderPath;
    shader_.reset();
    shaderError_ = false;
    if (shaderPath.empty()) {
        return;
    }
    try {
        shader_ = actorShaderCache().getOrLoad(shaderPath, [this]() {
            ShaderLoadResult result =
                ShaderLoader::load(shaderPath, sf::Shader::Type::Fragment);
            if (!result) {
                throw std::runtime_error(result.error);
            }
            return std::move(result.shader);
        });
    } catch (const std::exception& error) {
        shaderError_ = true;
        std::cerr << error.what() << '\n';
    }
}

void Actor::setShaderPath(const std::string& shaderPath) {
    this->shaderPath = shaderPath;
    loadedShaderPath_.clear();
    ensureShaderLoaded();
}

const std::string& Actor::getShaderPath() const {
    return shaderPath;
}

std::shared_ptr<sf::Shader> Actor::getShader() const {
    ensureShaderLoaded();
    return shader_;
}

bool Actor::hasShaderError() const {
    ensureShaderLoaded();
    return shaderError_;
}

bool Actor::getVisible() const {
    return visible_;
}

void Actor::setVisible(bool visible, bool applyToChildren) {
    visible_ = visible;
    if (applyToChildren) {
        for (const std::shared_ptr<Actor>& child : children_) {
            child->setVisible(visible, true);
        }
    }
}

bool Actor::getAnimatable() const {
    return animatable;
}

void Actor::setAnimatable(bool animate, bool applyToChildren) {
    animatable = animate;
    if (applyToChildren) {
        for (const std::shared_ptr<Actor>& child : children_) {
            child->setAnimatable(animate, true);
        }
    }
}

std::shared_ptr<sf::Texture> Actor::getSpriteTexture() const {
    return spriteTexture_;
}

std::shared_ptr<sf::Texture> Actor::getTexture() const {
    return texture_;
}

void Actor::setSpriteTexture(std::shared_ptr<sf::Texture> texture,
                             bool resetRect) {
    if (!texture) {
        throw std::invalid_argument("Actor texture must not be null");
    }
    spriteTexture_ = std::move(texture);
    sf::Sprite::setTexture(*spriteTexture_, resetRect);
    syncMapCache();
}

void Actor::setTexture(std::shared_ptr<sf::Texture> texture, bool resetRect) {
    texture_ = texture;
    setSpriteTexture(std::move(texture), resetRect);
}

void Actor::setTextureRect(const sf::IntRect& rectangle) {
    const sf::Vector2i previousSize = sf::Sprite::getTextureRect().size;
    sf::Sprite::setTextureRect(rectangle);
    if (previousSize != rectangle.size) {
        syncMapCache();
    }
}

sf::IntRect Actor::getTextureRect() const {
    return sf::Sprite::getTextureRect();
}

Material Actor::getMaterial() const {
    return material;
}

void Actor::setMaterial(const std::optional<Material>& material) {
    this->material = material.value_or(Material{});
}

float Actor::getLightBlock() const {
    return material.lightBlock;
}

void Actor::setLightBlock(float lightBlock) {
    material.lightBlock = lightBlock;
}

bool Actor::getMirror() const {
    return material.mirror;
}

void Actor::setMirror(bool mirror) {
    material.mirror = mirror;
}

float Actor::getReflectionStrength() const {
    return material.reflectionStrength;
}

void Actor::setReflectionStrength(float reflectionStrength) {
    material.reflectionStrength = reflectionStrength;
}

float Actor::getOpacity() const {
    return material.opacity;
}

void Actor::setOpacity(float opacity) {
    material.opacity = opacity;
}

bool Actor::getIgnoreLighting() const {
    return material.ignoreLighting;
}

void Actor::setIgnoreLighting(bool ignoreLighting) {
    material.ignoreLighting = ignoreLighting;
}

void Actor::_animate(float deltaTime) {
    if (!texture_) {
        return;
    }
    switchTimer_ += deltaTime;
    if (switchTimer_ < switchInterval) {
        return;
    }
    switchTimer_ = 0.0f;
    const sf::IntRect currentRect = getTextureRect();
    const unsigned int width = texture_->getSize().x;
    if (width == 0) {
        return;
    }
    const sf::IntRect nextRect({(currentRect.position.x + currentRect.size.x) %
                                    static_cast<int>(width),
                                currentRect.position.y},
                               currentRect.size);
    if (nextRect != currentRect) {
        setTextureRect(nextRect);
    }
}

void shutdownActorResources() noexcept {
    actorShaderCache().clear();
    blankTextureStorage().reset();
}

sf::Color Actor::getLightColour() const {
    return lightComp ? lightComp->lightColour : sf::Color::White;
}

void Actor::setLightColour(const sf::Color& colour) {
    if (!lightComp) {
        lightComp = std::make_shared<LightComponent>();
    }
    lightComp->lightColour = colour;
}

float Actor::getLightRadius() const {
    return lightComp ? lightComp->lightRadius : 16.0f;
}

void Actor::setLightRadius(float radius) {
    if (!lightComp) {
        lightComp = std::make_shared<LightComponent>();
    }
    lightComp->lightRadius = radius;
}
