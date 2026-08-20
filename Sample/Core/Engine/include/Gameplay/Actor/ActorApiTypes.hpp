#pragma once

#include <BindAnnotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Filters/SoundFilter.hpp>
#include <General/Material.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <SFML/Audio/Sound.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class Actor;

class LUDORK_ENGINE_API ActorAudioService {
public:
    virtual ~ActorAudioService();
    virtual std::shared_ptr<sf::Sound> playSoundEffect(
        const std::string& filename, const SoundFilter& filter) = 0;
    virtual void setSoundFilter(const std::shared_ptr<sf::Sound>& sound,
                                const SoundFilter& filter) = 0;
};

LUDORK_ENGINE_API void setActorAudioService(ActorAudioService* service);

BIND_CLASS(copyable = true, table_init = true)
struct AutoSoundParams {
    BIND_PROPERTY()
    float volume = 100.0f;

    BIND_PROPERTY()
    float minDistance = 64.0f;

    BIND_PROPERTY()
    float attenuation = 1.0f;

    BIND_PROPERTY()
    bool loop = false;

    BIND_PROPERTY()
    float maxDistance = 0.0f;
};

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API ActorMapService : public RuntimeObject {
public:
    ~ActorMapService() override;

    BIND_METHOD(Pure = true)
    virtual sf::Vector2u getSize() const = 0;

    BIND_METHOD(Pure = true)
    virtual bool isPassable(const Actor& actor,
                            const sf::Vector2i& position) const = 0;

    BIND_METHOD()
    virtual std::vector<Actor*> getCollision(Actor& actor,
                                             const sf::Vector2i& position) = 0;

    BIND_METHOD()
    virtual std::vector<Actor*> getOverlaps(Actor& actor) = 0;

    BIND_METHOD(Pure = true)
    virtual std::optional<Material> getTopMaterial(
        const sf::Vector2i& position) const = 0;

    BIND_METHOD()
    virtual void updateActorOccupancy(Actor& actor) = 0;

    BIND_METHOD()
    virtual void updateActorList() = 0;

    BIND_METHOD()
    virtual void destroyActor(Actor& actor) = 0;
};
