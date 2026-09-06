#pragma once

#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <optional>
#include <string>

namespace ludork::global::system_transition_impl {

float advanceElapsed(float elapsed, float duration, float deltaTime);
bool isComplete(float elapsed, float duration);

void cacheTransitionBackground();
void setTransition(const std::shared_ptr<sf::Texture>& transitionResource,
                   float transitionTime);
void freezeTransitionBackground();
bool isTransitionBackgroundFrozen();
bool isTransitionBackgroundFreezePending();
void cancelTransitionBackgroundFreeze();
void requestTransition(std::optional<std::string> transitionName,
                       float transitionTime);
void cancelPendingTransition();
bool isTransitionPending();
bool isInTransition();
void applyPendingTransition();

}  // namespace ludork::global::system_transition_impl
