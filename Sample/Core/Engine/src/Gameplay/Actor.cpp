#include <Gameplay/Actor.hpp>

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
