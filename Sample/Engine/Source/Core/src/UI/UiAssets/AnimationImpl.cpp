#include "AnimationSupport.hpp"
#include "AnimationImpl.hpp"

#include <Runtime/RuntimeDataReader.hpp>
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

using ludork::runtime::value_reader::findValue;
using ludork::runtime::value_reader::requireArray;
using ludork::runtime::value_reader::requireFloat;
using ludork::runtime::value_reader::requireInt;
using ludork::runtime::value_reader::requireMap;
using ludork::runtime::value_reader::requireString;

void requireOnlyKeys(const RuntimeData::Map& values,
                     const std::unordered_set<std::string>& allowed,
                     const std::string& source) {
    for (const auto& [name, value] : values) {
        static_cast<void>(value);
        if (!allowed.contains(name)) {
            throw std::invalid_argument(source + " has unknown field " + name);
        }
    }
}

sf::Vector2f requireVector2f(const RuntimeData& value,
                             const std::string& source) {
    const RuntimeData::Array& array = requireArray(value, source);
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

std::vector<AnimationDefinition::AnimationScalarKey> parseScalarKeys(
    const RuntimeData& value, float duration, const std::string& source,
    const std::function<void(float, const std::string&)>& validate) {
    const RuntimeData::Array& values = requireArray(value, source);
    if (values.empty()) {
        throw std::invalid_argument(source + " must contain at least one key");
    }
    std::vector<AnimationDefinition::AnimationScalarKey> result;
    result.reserve(values.size());
    float previous = -1.0f;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::string keySource =
            source + "[" + std::to_string(index) + "]";
        const RuntimeData::Map& key = requireMap(values[index], keySource);
        requireOnlyKeys(key, {"time", "value"}, keySource);
        const auto timeValue = findValue(key, "time");
        const auto dataValue = findValue(key, "value");
        if (!timeValue || !dataValue) {
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

std::vector<AnimationDefinition::AnimationVectorKey> parseVectorKeys(
    const RuntimeData& value, float duration, const std::string& source,
    const std::function<void(const sf::Vector2f&, const std::string&)>&
        validate) {
    const RuntimeData::Array& values = requireArray(value, source);
    if (values.empty()) {
        throw std::invalid_argument(source + " must contain at least one key");
    }
    std::vector<AnimationDefinition::AnimationVectorKey> result;
    result.reserve(values.size());
    float previous = -1.0f;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::string keySource =
            source + "[" + std::to_string(index) + "]";
        const RuntimeData::Map& key = requireMap(values[index], keySource);
        requireOnlyKeys(key, {"time", "value"}, keySource);
        const auto timeValue = findValue(key, "time");
        const auto dataValue = findValue(key, "value");
        if (!timeValue || !dataValue) {
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

std::vector<AnimationDefinition::AnimationColourKey> parseColourKeys(
    const RuntimeData& value, float duration, const std::string& source) {
    const RuntimeData::Array& values = requireArray(value, source);
    if (values.empty()) {
        throw std::invalid_argument(source + " must contain at least one key");
    }
    std::vector<AnimationDefinition::AnimationColourKey> result;
    result.reserve(values.size());
    float previous = -1.0f;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::string keySource =
            source + "[" + std::to_string(index) + "]";
        const RuntimeData::Map& key = requireMap(values[index], keySource);
        requireOnlyKeys(key, {"time", "value"}, keySource);
        const auto timeValue = findValue(key, "time");
        const auto dataValue = findValue(key, "value");
        if (!timeValue || !dataValue) {
            throw std::invalid_argument(keySource + " requires time and value");
        }
        const float time = requireFloat(*timeValue, keySource + ".time");
        if (time < 0.0f || time > duration || time <= previous) {
            throw std::invalid_argument(
                keySource + ".time must be strictly ordered within duration");
        }
        const RuntimeData::Array& components =
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

float evaluateScalar(
    const std::vector<AnimationDefinition::AnimationScalarKey>& keys,
    float time, float identity) {
    return evaluate(keys, time, identity,
                    [](float left, float right, float factor) {
                        return left + (right - left) * factor;
                    });
}

sf::Vector2f evaluateVector(
    const std::vector<AnimationDefinition::AnimationVectorKey>& keys,
    float time, const sf::Vector2f& identity) {
    return evaluate(
        keys, time, identity,
        [](const sf::Vector2f& left, const sf::Vector2f& right, float factor) {
            return left + (right - left) * factor;
        });
}

sf::Color evaluateColour(
    const std::vector<AnimationDefinition::AnimationColourKey>& keys,
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
    const std::shared_ptr<AssetImpl>& impl, const std::string& name,
    const std::optional<std::string>& target) {
    const auto iterator = impl->animations.find(definitionKey(name, target));
    return iterator == impl->animations.end() ? nullptr : iterator->second;
}

std::optional<ResolvedAnimation> resolve(
    const std::shared_ptr<AssetImpl>& impl, const std::string& name,
    const std::optional<std::string>& target) {
    if (impl == nullptr || name.empty()) {
        return std::nullopt;
    }
    if (target.has_value()) {
        const auto nested = impl->nestedImpls.find(*target);
        if (nested != impl->nestedImpls.end()) {
            return resolve(nested->second, name, std::nullopt);
        }
        const std::shared_ptr<const AnimationDefinition> definition =
            localDefinition(impl, name, target);
        const auto control = impl->controls.find(*target);
        if (definition == nullptr || control == impl->controls.end()) {
            return std::nullopt;
        }
        const std::string activeKey =
            control->second->control == impl->root->control ? "" : *target;
        return ResolvedAnimation{impl, definition, control->second->control,
                                 activeKey};
    }

    if (const std::shared_ptr<AssetImpl> parent = impl->parentImpl.lock()) {
        const std::shared_ptr<const AnimationDefinition> override =
            localDefinition(parent, name, impl->parentNodeName);
        if (override != nullptr) {
            return ResolvedAnimation{impl, override, impl->root->control, ""};
        }
    }
    const std::shared_ptr<const AnimationDefinition> definition =
        localDefinition(impl, name, std::nullopt);
    if (definition == nullptr) {
        return std::nullopt;
    }
    return ResolvedAnimation{impl, definition, impl->root->control, ""};
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

void update(const std::shared_ptr<AssetImpl>& impl, float deltaTime) {
    if (impl == nullptr || deltaTime < 0.0f) {
        return;
    }
    std::vector<PendingCallback> callbacks;
    std::vector<std::pair<std::string, std::shared_ptr<ActiveAnimation>>>
        snapshot;
    snapshot.reserve(impl->activeAnimations.size());
    for (const auto& [key, active] : impl->activeAnimations) {
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
        const auto iterator = impl->activeAnimations.find(pending.activeKey);
        if (iterator != impl->activeAnimations.end() &&
            iterator->second == pending.active) {
            pending.callback();
        }
    }
}

}  // namespace

void parseAnimations(const RuntimeData::Map& asset, AssetImpl& impl,
                     const std::string& source) {
    const auto animationsValue = findValue(asset, "animations");
    if (!animationsValue) {
        return;
    }
    const RuntimeData::Array& animations =
        requireArray(*animationsValue, source + ".animations");
    for (std::size_t index = 0; index < animations.size(); ++index) {
        const std::string animationSource =
            source + ".animations[" + std::to_string(index) + "]";
        const RuntimeData::Map& data =
            requireMap(animations[index], animationSource);
        requireOnlyKeys(data, {"name", "target", "duration", "pivot", "tracks"},
                        animationSource);
        const auto nameValue = findValue(data, "name");
        const auto targetValue = findValue(data, "target");
        const auto durationValue = findValue(data, "duration");
        const auto tracksValue = findValue(data, "tracks");
        if (!nameValue || !targetValue || !durationValue || !tracksValue) {
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
                (!impl.controls.contains(*definition->target) &&
                 !impl.nestedImpls.contains(*definition->target))) {
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
        if (const auto pivotValue = findValue(data, "pivot")) {
            definition->pivot =
                requireVector2f(*pivotValue, animationSource + ".pivot");
            if (definition->pivot.x < 0.0f || definition->pivot.x > 1.0f ||
                definition->pivot.y < 0.0f || definition->pivot.y > 1.0f) {
                throw std::invalid_argument(animationSource +
                                            ".pivot must be between 0 and 1");
            }
        }

        const RuntimeData::Map& tracks =
            requireMap(*tracksValue, animationSource + ".tracks");
        requireOnlyKeys(tracks, {"translation", "rotation", "scale", "colour"},
                        animationSource + ".tracks");
        if (const auto translation = findValue(tracks, "translation")) {
            definition->translation =
                parseVectorKeys(*translation, definition->duration,
                                animationSource + ".tracks.translation",
                                [](const sf::Vector2f&, const std::string&) {});
        }
        if (const auto rotation = findValue(tracks, "rotation")) {
            definition->rotation =
                parseScalarKeys(*rotation, definition->duration,
                                animationSource + ".tracks.rotation",
                                [](float, const std::string&) {});
        }
        if (const auto scale = findValue(tracks, "scale")) {
            definition->scale = parseVectorKeys(
                *scale, definition->duration, animationSource + ".tracks.scale",
                [](const sf::Vector2f& value, const std::string& field) {
                    if (value.x < 0.0f || value.y < 0.0f) {
                        throw std::invalid_argument(field +
                                                    " cannot be negative");
                    }
                });
        }
        if (const auto colour = findValue(tracks, "colour")) {
            definition->colour =
                parseColourKeys(*colour, definition->duration,
                                animationSource + ".tracks.colour");
        }

        const std::string key =
            definitionKey(definition->name, definition->target);
        if (!impl.animations.emplace(key, definition).second) {
            throw std::invalid_argument(
                animationSource + " duplicates an animation name and target");
        }
    }
}

void installAnimationUpdater(const std::shared_ptr<AssetImpl>& impl) {
    if (impl == nullptr || impl->root == nullptr ||
        impl->root->control == nullptr) {
        return;
    }
    const std::weak_ptr<AssetImpl> weakImpl = impl;
    impl->root->control->setPresentationUpdater([weakImpl](float deltaTime) {
        if (const std::shared_ptr<AssetImpl> owner = weakImpl.lock()) {
            update(owner, deltaTime);
        }
    });
    impl->root->control->setPresentationRelease([weakImpl]() {
        if (const std::shared_ptr<AssetImpl> owner = weakImpl.lock()) {
            stopAllAnimations(owner);
        }
    });
}

bool hasAnimation(const std::shared_ptr<AssetImpl>& impl,
                  const std::string& name,
                  const std::optional<std::string>& target) {
    return resolve(impl, name, target).has_value();
}

bool playAnimation(const std::shared_ptr<AssetImpl>& impl,
                   const std::string& name,
                   const std::optional<std::string>& target,
                   std::function<void()> onFinished) {
    const std::optional<ResolvedAnimation> resolved =
        resolve(impl, name, target);
    if (!resolved.has_value()) {
        std::cerr << "WARNING:UI animation \"" << name
                  << "\" was not found in \""
                  << (impl == nullptr ? std::string() : impl->assetKey)
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

void stopAnimation(const std::shared_ptr<AssetImpl>& impl,
                   const std::string& name,
                   const std::optional<std::string>& target) {
    const std::optional<ResolvedAnimation> resolved =
        resolve(impl, name, target);
    if (resolved.has_value()) {
        stopResolved(*resolved);
        return;
    }
    if (impl == nullptr) {
        return;
    }
    if (target.has_value()) {
        const auto nested = impl->nestedImpls.find(*target);
        if (nested != impl->nestedImpls.end()) {
            stopAnimation(nested->second, name, std::nullopt);
            return;
        }
    }
    const std::string activeKey = target.value_or("");
    const auto iterator = impl->activeAnimations.find(activeKey);
    if (iterator != impl->activeAnimations.end() &&
        iterator->second->definition->name == name) {
        clearAnimationPresentation(iterator->second);
        impl->activeAnimations.erase(iterator);
    }
}

bool sampleAnimation(const std::shared_ptr<AssetImpl>& impl,
                     const std::string& name,
                     const std::optional<std::string>& target, float time) {
    const std::optional<ResolvedAnimation> resolved =
        resolve(impl, name, target);
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

void stopAllAnimations(const std::shared_ptr<AssetImpl>& impl) {
    if (impl == nullptr) {
        return;
    }
    for (const auto& [key, active] : impl->activeAnimations) {
        static_cast<void>(key);
        clearAnimationPresentation(active);
    }
    impl->activeAnimations.clear();
    for (const auto& [name, nested] : impl->nestedImpls) {
        static_cast<void>(name);
        stopAllAnimations(nested);
    }
}

}  // namespace ludork::engine::ui_asset_runtime_impl
