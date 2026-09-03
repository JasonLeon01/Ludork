#include "AnimationRuntime.hpp"

#include <Runtime/RuntimeValueReader.hpp>
#include <UI/Canvas.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::ui_asset_runtime_impl {
namespace {

using ludork::engine::runtime_value_reader::findValue;
using ludork::engine::runtime_value_reader::requireArray;
using ludork::engine::runtime_value_reader::requireFloat;
using ludork::engine::runtime_value_reader::requireInt;
using ludork::engine::runtime_value_reader::requireMap;
using ludork::engine::runtime_value_reader::requireString;

struct ResolvedAnimation {
    std::shared_ptr<AssetState> owner;
    std::shared_ptr<const AnimationDefinition> definition;
    std::shared_ptr<ControlBase> target;
    std::string activeKey;
};

struct PendingCallback {
    std::string activeKey;
    std::shared_ptr<ActiveAnimation> active;
    std::function<void()> callback;
};

void requireOnlyKeys(const RuntimeValue::Map& values,
                     const std::unordered_set<std::string>& allowed,
                     const std::string& source) {
    for (const auto& [name, value] : values) {
        static_cast<void>(value);
        if (!allowed.contains(name)) {
            throw std::invalid_argument(source + " has unknown field " + name);
        }
    }
}

sf::Vector2f requireVector2f(const RuntimeValue& value,
                             const std::string& source) {
    const RuntimeValue::Array& array = requireArray(value, source);
    if (array.size() != 2) {
        throw std::invalid_argument(source + " must contain two numbers");
    }
    return {requireFloat(array[0], source + "[0]"),
            requireFloat(array[1], source + "[1]")};
}

std::string definitionKey(const std::string& name,
                          const std::optional<std::string>& target) {
    return target.value_or("") + '\x1f' + name;
}

bool isBlank(const std::string& value) {
    return value.empty() ||
           std::all_of(value.begin(), value.end(), [](unsigned char character) {
               return std::isspace(character) != 0;
           });
}

std::vector<AnimationScalarKey> parseScalarKeys(
    const RuntimeValue& value, float duration, const std::string& source,
    const std::function<void(float, const std::string&)>& validate) {
    const RuntimeValue::Array& values = requireArray(value, source);
    if (values.empty()) {
        throw std::invalid_argument(source + " must contain at least one key");
    }
    std::vector<AnimationScalarKey> result;
    result.reserve(values.size());
    float previous = -1.0f;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::string keySource =
            source + "[" + std::to_string(index) + "]";
        const RuntimeValue::Map& key = requireMap(values[index], keySource);
        requireOnlyKeys(key, {"time", "value"}, keySource);
        const RuntimeValue* timeValue = findValue(key, "time");
        const RuntimeValue* dataValue = findValue(key, "value");
        if (timeValue == nullptr || dataValue == nullptr) {
            throw std::invalid_argument(keySource + " requires time and value");
        }
        const float time = requireFloat(*timeValue, keySource + ".time");
        const float data = requireFloat(*dataValue, keySource + ".value");
        if (time < 0.0f || time > duration || time <= previous) {
            throw std::invalid_argument(
                keySource + ".time must be strictly ordered within duration");
        }
        validate(data, keySource + ".value");
        result.push_back({time, data});
        previous = time;
    }
    return result;
}

std::vector<AnimationVectorKey> parseVectorKeys(
    const RuntimeValue& value, float duration, const std::string& source,
    const std::function<void(const sf::Vector2f&, const std::string&)>&
        validate) {
    const RuntimeValue::Array& values = requireArray(value, source);
    if (values.empty()) {
        throw std::invalid_argument(source + " must contain at least one key");
    }
    std::vector<AnimationVectorKey> result;
    result.reserve(values.size());
    float previous = -1.0f;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::string keySource =
            source + "[" + std::to_string(index) + "]";
        const RuntimeValue::Map& key = requireMap(values[index], keySource);
        requireOnlyKeys(key, {"time", "value"}, keySource);
        const RuntimeValue* timeValue = findValue(key, "time");
        const RuntimeValue* dataValue = findValue(key, "value");
        if (timeValue == nullptr || dataValue == nullptr) {
            throw std::invalid_argument(keySource + " requires time and value");
        }
        const float time = requireFloat(*timeValue, keySource + ".time");
        const sf::Vector2f data =
            requireVector2f(*dataValue, keySource + ".value");
        if (time < 0.0f || time > duration || time <= previous) {
            throw std::invalid_argument(
                keySource + ".time must be strictly ordered within duration");
        }
        validate(data, keySource + ".value");
        result.push_back({time, data});
        previous = time;
    }
    return result;
}

std::vector<AnimationColourKey> parseColourKeys(const RuntimeValue& value,
                                                float duration,
                                                const std::string& source) {
    const RuntimeValue::Array& values = requireArray(value, source);
    if (values.empty()) {
        throw std::invalid_argument(source + " must contain at least one key");
    }
    std::vector<AnimationColourKey> result;
    result.reserve(values.size());
    float previous = -1.0f;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::string keySource =
            source + "[" + std::to_string(index) + "]";
        const RuntimeValue::Map& key = requireMap(values[index], keySource);
        requireOnlyKeys(key, {"time", "value"}, keySource);
        const RuntimeValue* timeValue = findValue(key, "time");
        const RuntimeValue* dataValue = findValue(key, "value");
        if (timeValue == nullptr || dataValue == nullptr) {
            throw std::invalid_argument(keySource + " requires time and value");
        }
        const float time = requireFloat(*timeValue, keySource + ".time");
        if (time < 0.0f || time > duration || time <= previous) {
            throw std::invalid_argument(
                keySource + ".time must be strictly ordered within duration");
        }
        const RuntimeValue::Array& components =
            requireArray(*dataValue, keySource + ".value");
        if (components.size() != 4) {
            throw std::invalid_argument(keySource +
                                        ".value must contain four RGBA values");
        }
        std::array<int, 4> channels;
        for (std::size_t channel = 0; channel < channels.size(); ++channel) {
            channels[channel] = requireInt(
                components[channel],
                keySource + ".value[" + std::to_string(channel) + "]");
            if (channels[channel] < 0 || channels[channel] > 255) {
                throw std::invalid_argument(
                    keySource + ".value components must be between 0 and 255");
            }
        }
        result.push_back(
            {time, sf::Color(static_cast<std::uint8_t>(channels[0]),
                             static_cast<std::uint8_t>(channels[1]),
                             static_cast<std::uint8_t>(channels[2]),
                             static_cast<std::uint8_t>(channels[3]))});
        previous = time;
    }
    return result;
}

template <typename Key, typename Value, typename Interpolate>
Value evaluate(const std::vector<Key>& keys, float time, const Value& identity,
               Interpolate interpolate) {
    if (keys.empty()) {
        return identity;
    }
    if (time <= keys.front().time) {
        return keys.front().value;
    }
    if (time >= keys.back().time) {
        return keys.back().value;
    }
    const auto upper = std::upper_bound(keys.begin(), keys.end(), time,
                                        [](float value, const Key& key) {
                                            return value < key.time;
                                        });
    const Key& right = *upper;
    const Key& left = *(upper - 1);
    const float factor = (time - left.time) / (right.time - left.time);
    return interpolate(left.value, right.value, factor);
}

float evaluateScalar(const std::vector<AnimationScalarKey>& keys, float time,
                     float identity) {
    return evaluate(keys, time, identity,
                    [](float left, float right, float factor) {
                        return left + (right - left) * factor;
                    });
}

sf::Vector2f evaluateVector(const std::vector<AnimationVectorKey>& keys,
                            float time, const sf::Vector2f& identity) {
    return evaluate(
        keys, time, identity,
        [](const sf::Vector2f& left, const sf::Vector2f& right, float factor) {
            return left + (right - left) * factor;
        });
}

sf::Color evaluateColour(const std::vector<AnimationColourKey>& keys,
                         float time) {
    return evaluate(
        keys, time, sf::Color::White,
        [](const sf::Color& left, const sf::Color& right, float factor) {
            const auto channel = [factor](std::uint8_t leftValue,
                                          std::uint8_t rightValue) {
                return static_cast<std::uint8_t>(
                    std::clamp(std::lround(static_cast<float>(leftValue) +
                                           (static_cast<float>(rightValue) -
                                            static_cast<float>(leftValue)) *
                                               factor),
                               0L, 255L));
            };
            return sf::Color(channel(left.r, right.r), channel(left.g, right.g),
                             channel(left.b, right.b),
                             channel(left.a, right.a));
        });
}

void applyColour(const std::shared_ptr<ControlBase>& control,
                 const void* source, const sf::Color& colour) {
    if (control == nullptr) {
        return;
    }
    control->setPresentationColour(source, colour);
    if (dynamic_cast<Canvas*>(control.get()) != nullptr) {
        return;
    }
    for (const std::shared_ptr<ControlBase>& child : control->getChildren()) {
        applyColour(child, source, colour);
    }
}

void clearColour(const std::shared_ptr<ControlBase>& control,
                 const void* source) {
    if (control == nullptr) {
        return;
    }
    control->clearPresentationColour(source);
    if (dynamic_cast<Canvas*>(control.get()) != nullptr) {
        return;
    }
    for (const std::shared_ptr<ControlBase>& child : control->getChildren()) {
        clearColour(child, source);
    }
}

void applyAnimationSample(const std::shared_ptr<ActiveAnimation>& active,
                          float time) {
    const AnimationDefinition& definition = *active->definition;
    active->target->setPresentationTransform(
        evaluateVector(definition.translation, time, {0.0f, 0.0f}),
        evaluateScalar(definition.rotation, time, 0.0f),
        evaluateVector(definition.scale, time, {1.0f, 1.0f}), definition.pivot);
    if (!definition.colour.empty()) {
        applyColour(active->target, active.get(),
                    evaluateColour(definition.colour, time));
    }
}

void clearAnimationPresentation(
    const std::shared_ptr<ActiveAnimation>& active) {
    if (active == nullptr || active->target == nullptr) {
        return;
    }
    active->target->resetPresentationTransform();
    clearColour(active->target, active.get());
    active->onFinished = {};
}

std::shared_ptr<const AnimationDefinition> localDefinition(
    const std::shared_ptr<AssetState>& state, const std::string& name,
    const std::optional<std::string>& target) {
    const auto iterator = state->animations.find(definitionKey(name, target));
    return iterator == state->animations.end() ? nullptr : iterator->second;
}

std::optional<ResolvedAnimation> resolve(
    const std::shared_ptr<AssetState>& state, const std::string& name,
    const std::optional<std::string>& target) {
    if (state == nullptr || name.empty()) {
        return std::nullopt;
    }
    if (target.has_value()) {
        const auto nested = state->nestedStates.find(*target);
        if (nested != state->nestedStates.end()) {
            return resolve(nested->second, name, std::nullopt);
        }
        const std::shared_ptr<const AnimationDefinition> definition =
            localDefinition(state, name, target);
        const auto control = state->controls.find(*target);
        if (definition == nullptr || control == state->controls.end()) {
            return std::nullopt;
        }
        const std::string activeKey =
            control->second->control == state->root->control ? "" : *target;
        return ResolvedAnimation{state, definition, control->second->control,
                                 activeKey};
    }

    if (const std::shared_ptr<AssetState> parent = state->parentState.lock()) {
        const std::shared_ptr<const AnimationDefinition> override =
            localDefinition(parent, name, state->parentNodeName);
        if (override != nullptr) {
            return ResolvedAnimation{state, override, state->root->control, ""};
        }
    }
    const std::shared_ptr<const AnimationDefinition> definition =
        localDefinition(state, name, std::nullopt);
    if (definition == nullptr) {
        return std::nullopt;
    }
    return ResolvedAnimation{state, definition, state->root->control, ""};
}

void clearActiveTarget(const ResolvedAnimation& resolved) {
    const auto iterator =
        resolved.owner->activeAnimations.find(resolved.activeKey);
    if (iterator == resolved.owner->activeAnimations.end()) {
        return;
    }
    clearAnimationPresentation(iterator->second);
    resolved.owner->activeAnimations.erase(iterator);
}

void stopResolved(const ResolvedAnimation& resolved) {
    const auto iterator =
        resolved.owner->activeAnimations.find(resolved.activeKey);
    if (iterator == resolved.owner->activeAnimations.end() ||
        iterator->second->definition->name != resolved.definition->name) {
        return;
    }
    clearActiveTarget(resolved);
}

void update(const std::shared_ptr<AssetState>& state, float deltaTime) {
    if (state == nullptr || deltaTime < 0.0f) {
        return;
    }
    std::vector<PendingCallback> callbacks;
    std::vector<std::pair<std::string, std::shared_ptr<ActiveAnimation>>>
        snapshot;
    snapshot.reserve(state->activeAnimations.size());
    for (const auto& [key, active] : state->activeAnimations) {
        snapshot.emplace_back(key, active);
    }
    for (const auto& [activeKey, active] : snapshot) {
        if (active == nullptr || !active->playing) {
            continue;
        }
        active->elapsed =
            std::min(active->definition->duration, active->elapsed + deltaTime);
        applyAnimationSample(active, active->elapsed);
        if (active->elapsed >= active->definition->duration) {
            active->playing = false;
            if (active->onFinished) {
                callbacks.push_back(
                    {activeKey, active, std::move(active->onFinished)});
            }
        }
    }
    for (const PendingCallback& pending : callbacks) {
        const auto iterator = state->activeAnimations.find(pending.activeKey);
        if (iterator != state->activeAnimations.end() &&
            iterator->second == pending.active) {
            pending.callback();
        }
    }
}

}  // namespace

void parseAnimations(const RuntimeValue::Map& asset, AssetState& state,
                     const std::string& source) {
    const RuntimeValue* animationsValue = findValue(asset, "animations");
    if (animationsValue == nullptr) {
        return;
    }
    const RuntimeValue::Array& animations =
        requireArray(*animationsValue, source + ".animations");
    for (std::size_t index = 0; index < animations.size(); ++index) {
        const std::string animationSource =
            source + ".animations[" + std::to_string(index) + "]";
        const RuntimeValue::Map& data =
            requireMap(animations[index], animationSource);
        requireOnlyKeys(data, {"name", "target", "duration", "pivot", "tracks"},
                        animationSource);
        const RuntimeValue* nameValue = findValue(data, "name");
        const RuntimeValue* targetValue = findValue(data, "target");
        const RuntimeValue* durationValue = findValue(data, "duration");
        const RuntimeValue* tracksValue = findValue(data, "tracks");
        if (nameValue == nullptr || targetValue == nullptr ||
            durationValue == nullptr || tracksValue == nullptr) {
            throw std::invalid_argument(
                animationSource +
                " requires name, target, duration, and tracks");
        }

        std::shared_ptr<AnimationDefinition> definition =
            std::make_shared<AnimationDefinition>();
        definition->name = requireString(*nameValue, animationSource + ".name");
        if (isBlank(definition->name)) {
            throw std::invalid_argument(animationSource +
                                        ".name cannot be empty");
        }
        if (!targetValue->isNil()) {
            definition->target =
                requireString(*targetValue, animationSource + ".target");
            if (definition->target->empty() ||
                (!state.controls.contains(*definition->target) &&
                 !state.nestedStates.contains(*definition->target))) {
                throw std::invalid_argument(animationSource +
                                            ".target is not a local UI node");
            }
        }
        definition->duration =
            requireFloat(*durationValue, animationSource + ".duration");
        if (definition->duration <= 0.0f) {
            throw std::invalid_argument(animationSource +
                                        ".duration must be positive");
        }
        if (const RuntimeValue* pivotValue = findValue(data, "pivot")) {
            definition->pivot =
                requireVector2f(*pivotValue, animationSource + ".pivot");
            if (definition->pivot.x < 0.0f || definition->pivot.x > 1.0f ||
                definition->pivot.y < 0.0f || definition->pivot.y > 1.0f) {
                throw std::invalid_argument(animationSource +
                                            ".pivot must be between 0 and 1");
            }
        }

        const RuntimeValue::Map& tracks =
            requireMap(*tracksValue, animationSource + ".tracks");
        requireOnlyKeys(tracks, {"translation", "rotation", "scale", "colour"},
                        animationSource + ".tracks");
        if (const RuntimeValue* translation =
                findValue(tracks, "translation")) {
            definition->translation =
                parseVectorKeys(*translation, definition->duration,
                                animationSource + ".tracks.translation",
                                [](const sf::Vector2f&, const std::string&) {});
        }
        if (const RuntimeValue* rotation = findValue(tracks, "rotation")) {
            definition->rotation =
                parseScalarKeys(*rotation, definition->duration,
                                animationSource + ".tracks.rotation",
                                [](float, const std::string&) {});
        }
        if (const RuntimeValue* scale = findValue(tracks, "scale")) {
            definition->scale = parseVectorKeys(
                *scale, definition->duration, animationSource + ".tracks.scale",
                [](const sf::Vector2f& value, const std::string& field) {
                    if (value.x < 0.0f || value.y < 0.0f) {
                        throw std::invalid_argument(field +
                                                    " cannot be negative");
                    }
                });
        }
        if (const RuntimeValue* colour = findValue(tracks, "colour")) {
            definition->colour =
                parseColourKeys(*colour, definition->duration,
                                animationSource + ".tracks.colour");
        }

        const std::string key =
            definitionKey(definition->name, definition->target);
        if (!state.animations.emplace(key, definition).second) {
            throw std::invalid_argument(
                animationSource + " duplicates an animation name and target");
        }
    }
}

void installAnimationUpdater(const std::shared_ptr<AssetState>& state) {
    if (state == nullptr || state->root == nullptr ||
        state->root->control == nullptr) {
        return;
    }
    const std::weak_ptr<AssetState> weakState = state;
    state->root->control->setPresentationUpdater([weakState](float deltaTime) {
        if (const std::shared_ptr<AssetState> owner = weakState.lock()) {
            update(owner, deltaTime);
        }
    });
    state->root->control->setPresentationRelease([weakState]() {
        if (const std::shared_ptr<AssetState> owner = weakState.lock()) {
            stopAllAnimations(owner);
        }
    });
}

bool hasAnimation(const std::shared_ptr<AssetState>& state,
                  const std::string& name,
                  const std::optional<std::string>& target) {
    return resolve(state, name, target).has_value();
}

bool playAnimation(const std::shared_ptr<AssetState>& state,
                   const std::string& name,
                   const std::optional<std::string>& target,
                   std::function<void()> onFinished) {
    const std::optional<ResolvedAnimation> resolved =
        resolve(state, name, target);
    if (!resolved.has_value()) {
        std::cerr << "WARNING:UI animation \"" << name
                  << "\" was not found in \""
                  << (state == nullptr ? std::string() : state->assetKey)
                  << "\" for target \"" << target.value_or("Global")
                  << "\"; skipped\n";
        if (onFinished) {
            onFinished();
        }
        return false;
    }
    clearActiveTarget(*resolved);
    std::shared_ptr<ActiveAnimation> active =
        std::make_shared<ActiveAnimation>();
    active->definition = resolved->definition;
    active->target = resolved->target;
    active->onFinished = std::move(onFinished);
    resolved->owner->activeAnimations.insert_or_assign(resolved->activeKey,
                                                       active);
    applyAnimationSample(active, 0.0f);
    return true;
}

void stopAnimation(const std::shared_ptr<AssetState>& state,
                   const std::string& name,
                   const std::optional<std::string>& target) {
    const std::optional<ResolvedAnimation> resolved =
        resolve(state, name, target);
    if (resolved.has_value()) {
        stopResolved(*resolved);
        return;
    }
    if (state == nullptr) {
        return;
    }
    if (target.has_value()) {
        const auto nested = state->nestedStates.find(*target);
        if (nested != state->nestedStates.end()) {
            stopAnimation(nested->second, name, std::nullopt);
            return;
        }
    }
    const std::string activeKey = target.value_or("");
    const auto iterator = state->activeAnimations.find(activeKey);
    if (iterator != state->activeAnimations.end() &&
        iterator->second->definition->name == name) {
        clearAnimationPresentation(iterator->second);
        state->activeAnimations.erase(iterator);
    }
}

bool sampleAnimation(const std::shared_ptr<AssetState>& state,
                     const std::string& name,
                     const std::optional<std::string>& target, float time) {
    const std::optional<ResolvedAnimation> resolved =
        resolve(state, name, target);
    if (!resolved.has_value()) {
        return false;
    }
    clearActiveTarget(*resolved);
    std::shared_ptr<ActiveAnimation> active =
        std::make_shared<ActiveAnimation>();
    active->definition = resolved->definition;
    active->target = resolved->target;
    active->elapsed = std::clamp(time, 0.0f, resolved->definition->duration);
    active->playing = false;
    resolved->owner->activeAnimations.insert_or_assign(resolved->activeKey,
                                                       active);
    applyAnimationSample(active, active->elapsed);
    return true;
}

void stopAllAnimations(const std::shared_ptr<AssetState>& state) {
    if (state == nullptr) {
        return;
    }
    for (const auto& [key, active] : state->activeAnimations) {
        static_cast<void>(key);
        clearAnimationPresentation(active);
    }
    state->activeAnimations.clear();
    for (const auto& [name, nested] : state->nestedStates) {
        static_cast<void>(name);
        stopAllAnimations(nested);
    }
}

}  // namespace ludork::engine::ui_asset_runtime_impl
