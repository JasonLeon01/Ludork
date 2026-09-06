#include <Gameplay/GameplayEventData.hpp>
#include "GameplayValueUtils.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

GameplayEventData::GameplayEventData(RuntimeValue eventInstigator,
                                     RuntimeValue eventTarget, std::string tag,
                                     RuntimeIdentityPtr eventPayload)
    : instigator(std::move(eventInstigator)),
      target(std::move(eventTarget)),
      eventTag(std::move(tag)),
      payload(std::move(eventPayload)) {
    if (payload == nullptr) {
        payload = ludork::global::gameplay_detail::runtimeMap();
    }
}
