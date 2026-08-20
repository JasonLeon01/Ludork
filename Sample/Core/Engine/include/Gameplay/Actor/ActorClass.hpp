#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Filters/SoundFilter.hpp>
#include <Gameplay/Actor/ActorApiTypes.hpp>
#include <Gameplay/BPBase.hpp>
#include <Gameplay/Components/LightComponent.hpp>
#include <General/Material.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <SFML/Audio/Sound.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

BIND_CLASS(
    bind_bases = false, runtime_bases = "BPBase,sf::Sprite",
    native_bases = "RuntimeObject,sf::Sprite",
    cast_bases = "RuntimeObject,sf::Sprite,sf::Drawable,sf::Transformable",
    callbacks =
        "update,lateUpdate,fixedUpdate,getPosition,getRelativePosition,"
        "getMapPosition,getOccupiedMapCells,getOccupiedMapCellsAtMapPosition,"
        "getCollisionEnabled,setCollisionEnabled,getPathfindingBlocks,"
        "setPathfindingBlocks,isDestroyed,markDestroyed,"
        "getRelativeMapPosition,getLocalBounds,getGlobalBounds,setPosition,"
        "setRelativePosition,setMapPosition,setRelativeMapPosition,move,"
        "getRotation,getRelativeRotation,setRotation,rotate,"
        "setRelativeRotation,getScale,getRelativeScale,setScale,scale,"
        "setRelativeScale,getOrigin,setOrigin,getTranslation,setTranslation,"
        "setAlignment,getMap,setMap,getParent,setParent,getChildren,addChild,"
        "removeChild,getVisible,setVisible,getAnimatable,setAnimatable,"
        "getSpriteTexture,getTexture,setSpriteTexture,setTexture,"
        "setTextureRect,getMaterial,setMaterial,getLightBlock,setLightBlock,"
        "getMirror,setMirror,getReflectionStrength,setReflectionStrength,"
        "getOpacity,setOpacity,getIgnoreLighting,setIgnoreLighting,setGraph,"
        "getGraph,hasGraph,getMapTag,setMapTag,ensureMapTag,syncMapCache,"
        "_superMove,_updatePositionFromParent,_updateRotationFromParent,"
        "_updateScaleFromParent,_animate,destroy,MapMove,onCreate,onTick,"
        "onLateTick,onFixedTick,onDestroy,onCollision,onOverlap,"
        "_getContinueMoveOffset,_onArrivedAtMapCell")
class LUDORK_ENGINE_API Actor : public RuntimeObject, public sf::Sprite {
public:
    BIND_INIT()
    explicit Actor(std::shared_ptr<sf::Texture> texture = nullptr,
                   std::optional<sf::IntRect> rect = std::nullopt,
                   std::string tag = "");

    ~Actor() override = default;

    BIND_PROPERTY(meta(BlueprintOnly = true))
    bool scriptMixin = false;

    BIND_PROPERTY(meta(Rely = {source = "scriptMixin", op = "==", value = true},
                       PathVars = "Scripts/Mixins", PathRoot = "Project",
                       PathFilter = "*.lua", BlueprintOnly = true))
    std::string scriptPath = "";

    BIND_PROPERTY()
    std::string tag = "";

    BIND_PROPERTY()
    float switchInterval = 0.2f;

    BIND_PROPERTY()
    bool animatable = false;

    BIND_PROPERTY(default = {})
    Material material;

    BIND_PROPERTY(meta(PathVars = "Shaders", PathFilter = "*.frag"))
    std::string shaderPath = "";

    BIND_PROPERTY(meta(ProgressVars = {0.0, 360.0, 1.0}))
    float hue = 0.0f;

    BIND_PROPERTY()
    bool collisionEnabled = false;

    BIND_METHOD(Pure = true)
    virtual bool getCollisionEnabled() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setCollisionEnabled(bool enabled);

    BIND_METHOD(Pure = true)
    virtual bool getPathfindingBlocks() const;

    BIND_METHOD()
    virtual void setPathfindingBlocks(bool blocks);

    BIND_METHOD(Pure = true)
    virtual bool isDestroyed() const;

    BIND_METHOD()
    virtual void markDestroyed(bool destroyed);

    BIND_METHOD(outpins(default = nil))
    void setShaderPath(const std::string& shaderPath);

    BIND_METHOD(Pure = true, returns = "shaderPath")
    const std::string& getShaderPath() const;

    BIND_METHOD(Pure = true, returns = "shader")
    std::shared_ptr<sf::Shader> getShader() const;

    BIND_METHOD(Pure = true, returns = "error")
    bool hasShaderError() const;

    BIND_METHOD()
    virtual void update(float deltaTime);

    BIND_METHOD()
    virtual void lateUpdate(float deltaTime);

    BIND_METHOD()
    virtual void fixedUpdate(float fixedDelta);

    BIND_METHOD(Pure = true, returns = "pos")
    virtual sf::Vector2f getPosition() const;

    BIND_METHOD(Pure = true, returns = "pos")
    virtual sf::Vector2f getRelativePosition() const;

    BIND_METHOD(Pure = true, returns = "pos")
    virtual sf::Vector2i getMapPosition() const;

    BIND_METHOD(Pure = true, defaults = {nil}, returns = "cells")
    virtual std::vector<sf::Vector2i> getOccupiedMapCells(
        const std::optional<sf::Vector2f>& worldPosition = std::nullopt) const;

    BIND_METHOD(Pure = true, returns = "cells")
    virtual std::vector<sf::Vector2i> getOccupiedMapCellsAtMapPosition(
        const sf::Vector2i& mapPosition) const;

    BIND_METHOD(Pure = true, returns = "pos")
    virtual sf::Vector2i getRelativeMapPosition() const;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getLocalBounds() const;

    BIND_METHOD(Pure = true)
    virtual sf::FloatRect getGlobalBounds() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setPosition(const sf::Vector2f& position);

    BIND_METHOD(outpins(default = nil))
    virtual void setRelativePosition(const sf::Vector2f& position);

    BIND_METHOD(outpins(default = nil))
    virtual void setMapPosition(const sf::Vector2u& position);

    void setMapPosition(const sf::Vector2i& position);

    BIND_METHOD(name = "setMapPosition", metadata = false)
    void setMapPositionSigned(const sf::Vector2i& position);

    BIND_METHOD(outpins(default = nil))
    virtual void setRelativeMapPosition(const sf::Vector2u& position);

    BIND_METHOD(outpins(default = nil))
    virtual void move(const sf::Vector2f& offset);

    BIND_METHOD(Pure = true, returns = "angle")
    virtual sf::Angle getRotation() const;

    BIND_METHOD(Pure = true, returns = "angle")
    virtual sf::Angle getRelativeRotation() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setRotation(sf::Angle angle);

    BIND_METHOD(name = "setRotation", metadata = false)
    void setRotationDegrees(float angle);

    BIND_METHOD(outpins(default = nil))
    virtual void rotate(sf::Angle angle);

    BIND_METHOD(name = "rotate", metadata = false)
    void rotateDegrees(float angle);

    BIND_METHOD(outpins(default = nil))
    virtual void setRelativeRotation(sf::Angle angle);

    BIND_METHOD(name = "setRelativeRotation", metadata = false)
    void setRelativeRotationDegrees(float angle);

    BIND_METHOD(Pure = true, returns = "scale")
    virtual sf::Vector2f getScale() const;

    BIND_METHOD(Pure = true, returns = "scale")
    virtual sf::Vector2f getRelativeScale() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setScale(const sf::Vector2f& factors);

    BIND_METHOD(outpins(default = nil))
    virtual void scale(const sf::Vector2f& factor);

    BIND_METHOD(outpins(default = nil))
    virtual void setRelativeScale(const sf::Vector2f& scale);

    BIND_METHOD(Pure = true, returns = "origin")
    virtual sf::Vector2f getOrigin() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setOrigin(const sf::Vector2f& origin);

    BIND_METHOD(Pure = true, returns = "translation")
    virtual sf::Vector2f getTranslation() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setTranslation(const sf::Vector2f& translation);

    BIND_METHOD(outpins(default = nil))
    virtual void setAlignment(const sf::Vector2f& alignment);

    BIND_METHOD(Pure = true, returns = "map_")
    virtual std::shared_ptr<ActorMapService> getMap() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setMap(const std::shared_ptr<ActorMapService>& inMap);

    BIND_METHOD(Pure = true)
    virtual std::shared_ptr<Actor> getParent() const;

    BIND_METHOD(allow_nil = "parent")
    virtual void setParent(const std::shared_ptr<Actor>& parent);

    BIND_METHOD(Pure = true)
    virtual const std::vector<std::shared_ptr<Actor>>& getChildren() const;

    BIND_METHOD(outpins(default = nil))
    virtual void addChild(const std::shared_ptr<Actor>& child);

    BIND_METHOD()
    virtual void removeChild(const std::shared_ptr<Actor>& child);

    BIND_METHOD(Pure = true, returns = "visible")
    virtual bool getVisible() const;

    BIND_METHOD(outpins(default = nil), defaults = {nil, true})
    virtual void setVisible(bool visible, bool applyToChildren = true);

    BIND_METHOD(Pure = true, returns = "animatable")
    virtual bool getAnimatable() const;

    BIND_METHOD(outpins(default = nil), defaults = {nil, true})
    virtual void setAnimatable(bool animate, bool applyToChildren = true);

    BIND_METHOD(Pure = true, returns = "texture")
    virtual std::shared_ptr<sf::Texture> getSpriteTexture() const;

    BIND_METHOD(Pure = true, returns = "texture")
    virtual std::shared_ptr<sf::Texture> getTexture() const;

    BIND_METHOD(outpins(default = nil), defaults = {nil, false})
    virtual void setSpriteTexture(std::shared_ptr<sf::Texture> texture,
                                  bool resetRect = false);

    BIND_METHOD(outpins(default = nil), defaults = {nil, false})
    virtual void setTexture(std::shared_ptr<sf::Texture> texture,
                            bool resetRect = false);

    BIND_METHOD()
    virtual void setTextureRect(const sf::IntRect& rectangle);

    BIND_METHOD(Pure = true)
    sf::IntRect getTextureRect() const;

    BIND_METHOD(Pure = true, returns = "material")
    virtual Material getMaterial() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setMaterial(const std::optional<Material>& material);

    BIND_METHOD(Pure = true, returns = "lightBlock")
    virtual float getLightBlock() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setLightBlock(float lightBlock);

    BIND_METHOD(Pure = true, returns = "mirror")
    virtual bool getMirror() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setMirror(bool mirror);

    BIND_METHOD(Pure = true, returns = "reflectionStrength")
    virtual float getReflectionStrength() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setReflectionStrength(float reflectionStrength);

    BIND_METHOD(Pure = true, returns = "opacity")
    virtual float getOpacity() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setOpacity(float opacity);

    BIND_METHOD(Pure = true, returns = "ignoreLighting")
    virtual bool getIgnoreLighting() const;

    BIND_METHOD(outpins(default = nil))
    virtual void setIgnoreLighting(bool ignoreLighting);

    BIND_METHOD(outpins(default = nil))
    virtual void setGraph(const RuntimeIdentityPtr& graph);

    BIND_METHOD(Pure = true)
    virtual RuntimeIdentityPtr getGraph() const;

    BIND_METHOD(Pure = true)
    virtual bool hasGraph() const;

    BIND_METHOD(Pure = true)
    virtual const std::string& getMapTag() const;

    BIND_METHOD()
    virtual void setMapTag(const std::string& value);

    BIND_METHOD()
    virtual void ensureMapTag();

    BIND_METHOD(metadata = false)
    virtual void syncMapCache() const;

    BIND_PROPERTY()
    bool tickable = false;

    BIND_PROPERTY()
    float speed = 64.0f;

    BIND_PROPERTY(meta(PathVars = "Sounds"))
    std::string autoSound = "";

    BIND_PROPERTY(meta(Rely = {source = "autoSound", op = "!=", value = ""}))
    float autoSoundInterval = 0.0f;

    BIND_METHOD(property = "autoSoundParams", type = "AutoSoundParams",
                setter = "setAutoSoundParams",
                default = {volume = 100.0, minDistance = 64.0,
                           attenuation = 1.0, loop = false, maxDistance = 0.0},
                meta(Rely = {source = "autoSound", op = "!=", value = ""}))
    std::shared_ptr<AutoSoundParams> getAutoSoundParams() const;

    void setAutoSoundParams(const AutoSoundParams& params);

    BIND_PROPERTY(meta(PathVars = "Characters"))
    std::string texturePath = "";

    BIND_PROPERTY(default = {{0, 0, 32, 32}},
                  meta(RectRangeVars = "texturePath"))
    sf::IntRect defaultRect{{0, 0}, {32, 32}};

    BIND_PROPERTY(default = {0.0, 0.0})
    sf::Vector2f defaultTranslation{0.0f, 0.0f};

    BIND_PROPERTY()
    float defaultRotation = 0.0f;

    BIND_PROPERTY(default = {1.0, 1.0})
    sf::Vector2f defaultScale{1.0f, 1.0f};

    BIND_PROPERTY(default = {0.0, 0.0})
    sf::Vector2f defaultOrigin{0.0f, 0.0f};

    BIND_PROPERTY(component = true)
    std::shared_ptr<LightComponent> lightComp;

    BIND_METHOD(property = "lightColour", setter = "setLightColour",
                metadata = false)
    sf::Color getLightColour() const;

    void setLightColour(const sf::Color& colour);

    BIND_METHOD(property = "lightRadius", setter = "setLightRadius",
                metadata = false)
    float getLightRadius() const;

    void setLightRadius(float radius);

    BIND_METHOD(metadata = false)
    void normaliseAutoSoundParams();

    BIND_REGISTER_EVENT()
    BIND_METHOD()
    virtual void onCreate();

    BIND_REGISTER_EVENT()
    BIND_METHOD()
    virtual void onTick(float deltaTime);

    BIND_REGISTER_EVENT()
    BIND_METHOD()
    virtual void onLateTick(float deltaTime);

    BIND_REGISTER_EVENT()
    BIND_METHOD()
    virtual void onFixedTick(float fixedDelta);

    BIND_REGISTER_EVENT()
    BIND_METHOD()
    virtual void onDestroy();

    BIND_REGISTER_EVENT()
    BIND_METHOD()
    virtual void onCollision(const std::vector<Actor*>& other);

    BIND_REGISTER_EVENT()
    BIND_METHOD()
    virtual void onOverlap(const std::vector<Actor*>& other);

    BIND_METHOD(outpins(default = nil))
    virtual void destroy();

    BIND_METHOD(outpins(success = true, fail = false))
    virtual bool MapMove(const sf::Vector2i& offset);

    BIND_METHOD(Pure = true, returns = "tickable")
    bool getTickable() const;

    BIND_METHOD(outpins(default = nil), defaults = {nil, true})
    void setTickable(bool tickable, bool applyToChildren = true);

    BIND_METHOD(Pure = true, returns = "intersects")
    bool intersects(const Actor& other) const;

    BIND_METHOD(Pure = true, returns = "isMoving")
    bool isMoving() const;

    BIND_METHOD(Pure = true, returns = "isInRoute")
    bool isInRoute() const;

    BIND_METHOD(defaults = {{}}, meta(MoveRouteVars = {"route"}),
                outpins(default = nil))
    void setRoute(const std::optional<std::vector<sf::Vector2i>>& route =
                      std::vector<sf::Vector2i>{});

    BIND_METHOD(Pure = true, returns = "route")
    std::optional<std::vector<sf::Vector2i>> getRoute() const;

    BIND_METHOD(Pure = true, returns = "moveEnabled")
    bool getMoveEnabled() const;

    BIND_METHOD(outpins(default = nil))
    void setMoveEnabled(bool enabled);

    BIND_METHOD(outpins(default = nil))
    void stop();

    BIND_METHOD(Pure = true, returns = "velocity")
    std::optional<sf::Vector2f> getVelocity() const;

    BIND_METHOD(defaults = {nil, nil})
    static void BlueprintEvent(const RuntimeIdentityPtr& object,
                               const RuntimeIdentityPtr& objectType,
                               const std::string& eventName,
                               const RuntimeValue& keywordArguments = {},
                               const RuntimeIdentityPtr& onComplete = nullptr);

    BIND_METHOD()
    static bool HasBlueprintEvent(const RuntimeIdentityPtr& object,
                                  const std::string& eventName);

    BIND_METHOD()
    static bool IsBlueprintEventEmpty(const RuntimeIdentityPtr& object,
                                      const std::string& eventName);

    BIND_METHOD(defaults = {nil, nil, nil})
    static RuntimeValue GenActor(
        const RuntimeIdentityPtr& actorModel,
        const RuntimeIdentityPtr& texture = nullptr,
        const RuntimeValue& textureRect = {},
        const std::optional<std::string>& tag = std::nullopt);

    BIND_METHOD(metadata = false)
    void refreshDescendantCache();

    BIND_IGNORE()
    const std::unordered_set<Actor*>& getDescendantActors() const;

    BIND_IGNORE()
    bool blocksPassability() const;

protected:
    BIND_METHOD(metadata = false)
    virtual void _superMove(const sf::Vector2f& offset);

    BIND_METHOD(metadata = false)
    virtual void _updatePositionFromParent();

    BIND_METHOD(metadata = false)
    virtual void _updateRotationFromParent();

    BIND_METHOD(metadata = false)
    virtual void _updateScaleFromParent();

    BIND_METHOD(metadata = false)
    virtual void _animate(float deltaTime);

    BIND_METHOD(metadata = false)
    virtual std::optional<sf::Vector2i> _getContinueMoveOffset();

    BIND_METHOD(metadata = false)
    virtual void _onArrivedAtMapCell();

    float processMoving(float deltaTime);
    void tryStartNextRouteStep();
    void autoFixMapPosition();
    float autoSoundListenerDistance() const;
    void updateAutoSound(float deltaTime);
    void playAutoSound();
    void stopAutoSound();
    void applyAutoSoundParams();
    SoundFilter buildAutoSoundFilter() const;

private:
    static const sf::Texture& textureOrBlank(
        const std::shared_ptr<sf::Texture>& texture);

    void ensureShaderLoaded() const;

    std::vector<sf::Vector2i> computeOccupiedCells(
        const sf::FloatRect& bounds) const;

    static RuntimeValue actorListValue(const std::vector<Actor*>& actors);

    std::weak_ptr<ActorMapService> map_;
    sf::Vector2f cachedPosition_;
    sf::Vector2i cachedMapPosition_;
    sf::FloatRect cachedGlobalBounds_;
    std::vector<sf::Vector2i> occupiedCells_;
    bool pathfindingBlocks_ = false;
    std::unordered_set<Actor*> descendantActors_;
    std::weak_ptr<Actor> parent_;
    std::vector<std::shared_ptr<Actor>> children_;
    sf::Vector2f translation_;
    sf::Vector2f relativePosition_;
    sf::Angle relativeRotation_;
    sf::Vector2f relativeScale_{1.0f, 1.0f};
    std::shared_ptr<sf::Texture> texture_;
    std::shared_ptr<sf::Texture> spriteTexture_;
    float switchTimer_ = 0.0f;
    bool visible_ = true;
    RuntimeIdentityPtr graph_;
    mutable std::shared_ptr<sf::Shader> shader_;
    mutable bool shaderError_ = false;
    mutable std::string loadedShaderPath_;
    std::string mapTag_;
    bool destroyed_ = false;

    bool moving_ = false;
    bool inRoute_ = false;
    std::optional<std::vector<sf::Vector2i>> route_ =
        std::vector<sf::Vector2i>{};
    bool moveEnabled_ = true;
    std::optional<sf::Vector2f> departure_;
    std::optional<sf::Vector2f> destination_;
    std::optional<sf::Vector2i> moveOriginMapPosition_;
    float realSpeed_ = 0.0f;
    std::shared_ptr<sf::Sound> autoSoundObject_;
    float autoSoundCooldown_ = 0.0f;
    std::optional<sf::Vector3f> autoSoundLastPosition_;
    std::shared_ptr<AutoSoundParams> autoSoundParams_ =
        std::make_shared<AutoSoundParams>();
};
