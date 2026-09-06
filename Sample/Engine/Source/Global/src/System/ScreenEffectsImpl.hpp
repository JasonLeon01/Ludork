#pragma once

#include <SFML/Graphics.hpp>

#include <optional>

namespace ludork::global::system_screen_effects_impl {

sf::Glsl::Vec4 makeToneColour(float red, float green, float blue, float gray);
sf::Glsl::Vec4 interpolateTone(const sf::Glsl::Vec4& start,
                               const sf::Glsl::Vec4& target, float ratio);
bool isNeutralTone(const sf::Glsl::Vec4& colour);

void applyScreenTonePass();
void flashScreen(std::optional<sf::Color> color, float duration);
void stopFlash();
bool isFlashing();
void changeScreenTone(float red, float green, float blue, float gray,
                      float duration);
void clearScreenTone(float duration);
void stopScreenTone();
bool isScreenToneActive();
bool isScreenToneTransitionComplete();
void startShake(float power, float speed, float duration);
void stopShake();
bool isShaking();
void updateFlash(float deltaTime);
void updateScreenTone(float deltaTime);
void updateShake(float deltaTime);
bool ensureToneShader();
void applyScreenToneUniform();
void ensureToneBuffer(const sf::Vector2u& size);

}  // namespace ludork::global::system_screen_effects_impl
