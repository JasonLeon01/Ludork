#include <Gameplay/Actor.hpp>

#include "Actor/ActorRuntime.hpp"
#include "Actor/AudioRuntime.hpp"
#include "Actor/AudioService.hpp"
#include "Actor/MovementRuntime.hpp"
#include "Actor/SpatialRuntime.hpp"
#include "Actor/VisualRuntime.hpp"

#include <Runtime/Blueprint/BPBase.hpp>
#include <EngineState.hpp>
#include <Runtime/RuntimeReflection.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

RuntimeValue optionalStringValue(const std::optional<std::string>& value) {
    return value.has_value() ? RuntimeValue(*value) : RuntimeValue();
}

}  // namespace

Actor::RuntimeHandle::RuntimeHandle()
    : state_(std::make_unique<ludork::engine::actor_impl::ActorRuntime>()) {}

Actor::RuntimeHandle::~RuntimeHandle() = default;

Actor::RuntimeHandle::RuntimeHandle(const RuntimeHandle& other)
    : state_(std::make_unique<ludork::engine::actor_impl::ActorRuntime>(
          other.get())) {}

Actor::RuntimeHandle& Actor::RuntimeHandle::operator=(
    const RuntimeHandle& other) {
    if (this != &other) {
        state_ = std::make_unique<ludork::engine::actor_impl::ActorRuntime>(
            other.get());
    }
    return *this;
}

Actor::RuntimeHandle::RuntimeHandle(RuntimeHandle&& other) noexcept = default;

Actor::RuntimeHandle& Actor::RuntimeHandle::operator=(
    RuntimeHandle&& other) noexcept = default;

ludork::engine::actor_impl::ActorRuntime& Actor::RuntimeHandle::get() noexcept {
    return *state_;
}

const ludork::engine::actor_impl::ActorRuntime& Actor::RuntimeHandle::get()
    const noexcept {
    return *state_;
}

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
        const RuntimeValue type =
            runtimeReflection().typeOf(RuntimeValue(self));
        const RuntimeValue value = runtimeReflection().get(
            ludork::runtime::reference::intern(type), "_GENERATED_CLASS");
        const bool* flag = value.getIf<bool>();
        generatedClass = flag != nullptr && *flag;
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
                           const RuntimeIdentityPtr& keywordArguments,
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
    RuntimeValue result = runtimeReflection().construct(
        ludork::runtime::reference::intern(RuntimeValue(actorModel)),
        {RuntimeValue(texture), textureRect, optionalStringValue(tag)});
    if (result.isNil()) {
        throw std::runtime_error(
            "Actor runtime construction returned no instance");
    }
    return result;
}

std::shared_ptr<AutoSoundParams> Actor::getAutoSoundParams() const {
    return runtime_.get().autoSoundParams;
}

void Actor::setAutoSoundParams(const AutoSoundParams& params) {
    runtime_.get().autoSoundParams = std::make_shared<AutoSoundParams>(params);
}

void Actor::normaliseAutoSoundParams() {
    std::shared_ptr<AutoSoundParams>& autoSoundParams =
        runtime_.get().autoSoundParams;
    autoSoundParams = autoSoundParams
                          ? std::make_shared<AutoSoundParams>(*autoSoundParams)
                          : std::make_shared<AutoSoundParams>();
}

float Actor::autoSoundListenerDistance() const {
    return ludork::engine::actor_impl::listenerDistance(getPosition());
}

void Actor::updateAutoSound(float deltaTime) {
    ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    if (autoSound.empty()) {
        stopAutoSound();
        runtime.autoSoundCooldown = 0.0f;
        return;
    }
    const float stopDistance = runtime.autoSoundParams->maxDistance;
    if (stopDistance > 0.0f) {
        const float distance = autoSoundListenerDistance();
        const float startDistance = stopDistance * 0.85f;
        if (distance > stopDistance) {
            stopAutoSound();
            runtime.autoSoundCooldown = 0.0f;
            return;
        }
        if (!runtime.autoSoundObject && distance > startDistance) {
            return;
        }
    }
    if (runtime.autoSoundObject) {
        if (runtime.autoSoundObject->getStatus() ==
            sf::SoundSource::Status::Stopped) {
            runtime.autoSoundObject.reset();
            runtime.autoSoundCooldown = std::max(0.0f, autoSoundInterval);
        } else {
            applyAutoSoundParams();
            return;
        }
    }
    if (runtime.autoSoundCooldown > 0.0f) {
        runtime.autoSoundCooldown =
            std::max(0.0f, runtime.autoSoundCooldown - deltaTime);
        return;
    }
    playAutoSound();
}

void Actor::playAutoSound() {
    if (actorAudioService() == nullptr) {
        return;
    }
    ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    runtime.autoSoundObject =
        actorAudioService()->playSoundEffect(autoSound, buildAutoSoundFilter());
    if (!runtime.autoSoundObject) {
        return;
    }
    const sf::Vector2f position = getPosition();
    runtime.autoSoundLastPosition = sf::Vector3f(position.x, position.y, 0.0f);
}

void Actor::stopAutoSound() {
    ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    if (!runtime.autoSoundObject) {
        return;
    }
    if (runtime.autoSoundObject->getStatus() !=
        sf::SoundSource::Status::Stopped) {
        runtime.autoSoundObject->stop();
    }
    runtime.autoSoundObject.reset();
    runtime.autoSoundLastPosition.reset();
}

void Actor::applyAutoSoundParams() {
    ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    if (!runtime.autoSoundObject || actorAudioService() == nullptr) {
        return;
    }
    const sf::Vector2f position = getPosition();
    const sf::Vector3f newPosition(position.x, position.y, 0.0f);
    if (runtime.autoSoundLastPosition.has_value() &&
        *runtime.autoSoundLastPosition == newPosition) {
        return;
    }
    runtime.autoSoundLastPosition = newPosition;
    actorAudioService()->setSoundFilter(runtime.autoSoundObject,
                                        buildAutoSoundFilter());
}

SoundFilter Actor::buildAutoSoundFilter() const {
    return ludork::engine::actor_impl::buildSoundFilter(
        *runtime_.get().autoSoundParams, getPosition());
}

void Actor::fixedUpdate(float fixedDelta) {
    ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    const sf::Vector2f start = getPosition();
    float remaining = fixedDelta;
    while (remaining > 0.0f) {
        if (!runtime.moving) {
            tryStartNextRouteStep();
        }
        if (!runtime.moving) {
            const std::optional<sf::Vector2i> offset = _getContinueMoveOffset();
            if (offset.has_value()) {
                MapMove(*offset);
            }
        }
        if (!runtime.moving) {
            break;
        }
        remaining = processMoving(remaining);
    }
    const float distance = (getPosition() - start).length();
    runtime.realSpeed =
        fixedDelta <= 0.0f || distance <= 0.001f ? 0.0f : distance / fixedDelta;
}

bool Actor::MapMove(const sf::Vector2i& requestedOffset) {
    ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    const std::shared_ptr<ActorMapService> map = getMap();
    if (!runtime.moveEnabled || !map || runtime.moving) {
        return false;
    }
    const sf::Vector2i offset =
        ludork::engine::actor_impl::normaliseDirection(requestedOffset);
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
    runtime.moving = true;
    runtime.moveOriginMapPosition = getMapPosition();
    runtime.departure = getPosition();
    runtime.destination = *runtime.departure +
                          sf::Vector2f(static_cast<float>(offset.x * CellSize),
                                       static_cast<float>(offset.y * CellSize));
    return true;
}

bool Actor::isMoving() const {
    const ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    return runtime.moving || runtime.realSpeed > 0.0f || runtime.inRoute;
}

sf::Vector2i Actor::getMapPosition() const {
    const ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    syncMapCache();
    if (runtime.moving && runtime.moveOriginMapPosition.has_value()) {
        return *runtime.moveOriginMapPosition;
    }
    return cachedMapPosition_;
}

bool Actor::isInRoute() const {
    return runtime_.get().inRoute;
}

void Actor::setRoute(const std::optional<std::vector<sf::Vector2i>>& route) {
    ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    runtime.route = route;
    runtime.inRoute = runtime.route.has_value() && !runtime.route->empty();
}

std::optional<std::vector<sf::Vector2i>> Actor::getRoute() const {
    return runtime_.get().route;
}

bool Actor::getMoveEnabled() const {
    return runtime_.get().moveEnabled;
}

void Actor::setMoveEnabled(bool enabled) {
    runtime_.get().moveEnabled = enabled;
    if (!runtime_.get().moveEnabled) {
        stop();
    }
}

void Actor::stop() {
    ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    runtime.moving = false;
    runtime.inRoute = false;
    runtime.route.reset();
    runtime.departure.reset();
    runtime.destination.reset();
    runtime.moveOriginMapPosition.reset();
    runtime.realSpeed = 0.0f;
    autoFixMapPosition();
}

std::optional<sf::Vector2f> Actor::getVelocity() const {
    const std::shared_ptr<ActorMapService> map = getMap();
    if (!map) {
        return std::nullopt;
    }
    const std::optional<Material> topMaterial =
        map->getTopMaterial(getMapPosition());
    const ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    return ludork::engine::actor_impl::movementVelocity(
        runtime.departure, runtime.destination, speed,
        topMaterial.has_value() ? topMaterial->speedRate : 1.0f);
}

std::optional<sf::Vector2i> Actor::_getContinueMoveOffset() {
    return std::nullopt;
}

void Actor::_onArrivedAtMapCell() {}

float Actor::processMoving(float deltaTime) {
    ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    const std::optional<sf::Vector2f> velocity = getVelocity();
    const std::shared_ptr<ActorMapService> map = getMap();
    if (!velocity.has_value() || !runtime.destination.has_value() ||
        !runtime.departure.has_value() || !map) {
        return 0.0f;
    }
    const ludork::engine::actor_impl::MovementAdvance advance =
        ludork::engine::actor_impl::advanceMovement(
            getPosition(), *runtime.destination, *velocity, deltaTime);
    if (!advance.completed) {
        move(advance.position - getPosition());
        return 0.0f;
    }
    setPosition(advance.position);
    runtime.moving = false;
    runtime.departure.reset();
    runtime.destination.reset();
    runtime.moveOriginMapPosition.reset();
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
    return advance.remainingTime;
}

void Actor::tryStartNextRouteStep() {
    ludork::engine::actor_impl::ActorRuntime& runtime = runtime_.get();
    if (!runtime.inRoute) {
        return;
    }
    if (!runtime.route.has_value() || runtime.route->empty()) {
        runtime.inRoute = false;
        return;
    }
    const sf::Vector2i step = runtime.route->front();
    runtime.route->erase(runtime.route->begin());
    if (!MapMove(step)) {
        runtime.inRoute = false;
        runtime.route = std::vector<sf::Vector2i>{};
    }
}

void Actor::autoFixMapPosition() {
    const sf::Vector2f position = getPosition();
    runtime_.get().moveOriginMapPosition.reset();
    setMapPosition(
        ludork::engine::actor_impl::snappedMapPosition(position, CellSize));
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
    const sf::Vector2i mapDelta(ludork::engine::actor_impl::roundHalfToEven(
                                    delta.x / static_cast<float>(CellSize)),
                                ludork::engine::actor_impl::roundHalfToEven(
                                    delta.y / static_cast<float>(CellSize)));
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
    map_ = ludork::runtime::detail::canonicalRuntimeOwner(inMap);
}

std::shared_ptr<Actor> Actor::getParent() const {
    return parent_.lock();
}

void Actor::setParent(const std::shared_ptr<Actor>& parent) {
    parent_ = ludork::runtime::detail::canonicalRuntimeOwner(parent);
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
    self->cachedMapPosition_ = ludork::engine::actor_impl::mapPosition(
        self->cachedPosition_, CellSize);
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
    return ludork::engine::actor_impl::occupiedCells(bounds, cachedMapPosition_,
                                                     CellSize);
}

const sf::Texture& Actor::textureOrBlank(
    const std::shared_ptr<sf::Texture>& texture) {
    return ludork::engine::actor_impl::textureOrBlank(texture);
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
    const ludork::engine::actor_impl::ShaderResult result =
        ludork::engine::actor_impl::loadShader(shaderPath);
    shader_ = result.shader;
    shaderError_ = result.failed;
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
    const sf::IntRect nextRect = ludork::engine::actor_impl::nextAnimationRect(
        currentRect, texture_->getSize().x);
    if (nextRect != currentRect) {
        setTextureRect(nextRect);
    }
}

void shutdownActorResources() noexcept {
    ludork::engine::actor_impl::shutdownVisualResources();
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
