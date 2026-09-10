#pragma once

#include <Filters/SoundFilter.hpp>
#include <Gameplay/AutoSoundParams.hpp>

#include <SFML/Audio/Sound.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ludork::engine::actor_impl {

class AudioImpl {
public:
    using PositionReader = std::function<sf::Vector2f()>;

    std::shared_ptr<AutoSoundParams> getParams() const;
    void setParams(const AutoSoundParams& params);
    void normaliseParams();
    void update(float deltaTime, const std::string& sound,
                const float& interval, const PositionReader& readPosition);
    void play(const std::string& sound, const PositionReader& readPosition);
    void stop();
    void applyParams(const PositionReader& readPosition);
    SoundFilter buildFilter(const PositionReader& readPosition) const;

private:
    std::shared_ptr<sf::Sound> sound_;
    float cooldown_ = 0.0f;
    std::optional<sf::Vector3f> lastPosition_;
    std::shared_ptr<AutoSoundParams> params_ =
        std::make_shared<AutoSoundParams>();
};

float listenerDistance(const sf::Vector2f& position);
SoundFilter buildSoundFilter(const AutoSoundParams& params,
                             const sf::Vector2f& position);

}  // namespace ludork::engine::actor_impl
