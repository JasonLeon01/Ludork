#pragma once

#include <CoreMinimal.hpp>
#include <Emitters/Emitter.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/Components/Component.hpp>

class Actor;
class EmitterScheduler;

BIND_CLASS(table_init = true)
class LUDORK_ENGINE_API EmitterComponent : public Component {
public:
    BIND_INIT()
    EmitterComponent() = default;
    ~EmitterComponent() override;

    BIND_METHOD()
    RuntimeValue::Array onAttach(const RuntimeIdentityPtr& owner) override;

    BIND_METHOD()
    std::shared_ptr<Emitter> getEmitter();

    BIND_PROPERTY(default = "", meta(GeneralDataVars = PARTICLE))
    std::string resource;

    BIND_PROPERTY(default = {0.5, 0.5})
    sf::Vector2f anchor{0.5f, 0.5f};

    BIND_PROPERTY(default = {0.0, 0.0})
    sf::Vector2f offset{0.0f, 0.0f};

    BIND_PROPERTY()
    float rotation = 0.0f;

    BIND_PROPERTY(default = {1.0, 1.0})
    sf::Vector2f scale{1.0f, 1.0f};

    BIND_PROPERTY()
    bool beforeActor = false;

    void collect(Actor& owner, EmitterScheduler& scheduler);
    void draw(Actor& owner, sf::RenderTarget& target, sf::RenderStates states,
              bool before);
    void release() noexcept;

private:
    std::weak_ptr<Actor> owner_;
    std::shared_ptr<Emitter> emitter_;
    std::string loadedResource_;
};
