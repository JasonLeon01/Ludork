#pragma once

#include <CoreMinimal.hpp>
#include <EngineRuntimeApi.hpp>
#include <Emitters/EmitterConfiguration.hpp>
#include <Runtime/Graphics/EmitterStatistics.hpp>

BIND_CLASS()
class LUDORK_ENGINE_API Emitter : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(Emitter, RuntimeObject)

    BIND_INIT(defaults = {""})
    explicit Emitter(const std::string& resourceKey = "");
    ~Emitter() override;

    BIND_METHOD()
    void load(const std::string& resourceKey);
    BIND_METHOD(metadata = false)
    void setConfiguration(const EmitterConfiguration& configuration);
    BIND_METHOD(metadata = false)
    EmitterConfiguration getConfiguration() const;
    BIND_METHOD()
    void play();
    BIND_METHOD()
    void pause();
    BIND_METHOD()
    void resume();
    BIND_METHOD()
    void restart();
    BIND_METHOD(defaults = {true})
    void stop(bool clear = true);
    BIND_METHOD()
    void emit(const std::string& trackName, int count);
    BIND_METHOD()
    void setSpeed(float speed);
    BIND_METHOD()
    void setColour(const sf::Color& colour);
    BIND_METHOD(Pure = true)
    float getSpeed() const;
    BIND_METHOD(Pure = true)
    float getTime() const;
    BIND_METHOD(Pure = true)
    bool isPlaying() const;
    BIND_METHOD(Pure = true)
    int getCapacity() const;
    BIND_METHOD(metadata = false)
    ludork::runtime::graphics::EmitterStatistics getStatistics() const;
    BIND_METHOD()
    void setProfiling(bool enabled);

    void tick(float deltaTime);
    void draw(sf::RenderTarget& target, sf::RenderStates states = {});
    void setHostTransform(const sf::Transform& transform);
    void resetHostMotion();
    bool isWarming() const;
    void shutdown() noexcept;
    static void collectGarbage() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
