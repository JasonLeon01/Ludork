#pragma once

#include <RuntimeApi.hpp>
#include <Runtime/Graphics/EmitterStatistics.hpp>
#include <Runtime/Graphics/GpuEmitterConfiguration.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <memory>
#include <string>

namespace sf {
class RenderTarget;
}

namespace ludork::runtime::graphics {

class LUDORK_RUNTIME_API GpuEmitterBackend {
public:
    GpuEmitterBackend();
    ~GpuEmitterBackend();
    GpuEmitterBackend(const GpuEmitterBackend&) = delete;
    GpuEmitterBackend& operator=(const GpuEmitterBackend&) = delete;

    void setConfiguration(GpuEmitterConfiguration configuration);
    void play();
    void pause();
    void resume();
    void restart();
    void stop(bool clear);
    void emit(const std::string& trackName, int count);
    void setSpeed(float speed);
    void setColour(const sf::Color& colour);
    float getSpeed() const;
    float getTime() const;
    bool isPlaying() const;
    int getCapacity() const;
    EmitterStatistics getStatistics() const;
    void setProfiling(bool enabled);
    void tick(float deltaTime);
    void draw(sf::RenderTarget& target, sf::RenderStates states);
    void setHostTransform(const sf::Transform& transform);
    void resetHostMotion();
    bool isWarming() const;
    void shutdown() noexcept;
    static void collectGarbage() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ludork::runtime::graphics
