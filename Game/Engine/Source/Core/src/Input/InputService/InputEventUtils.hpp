#pragma once

#include <optional>
#include <unordered_map>

namespace ludork::engine::input_impl {

template <typename Key>
std::optional<bool> consumeInputEvent(std::unordered_map<Key, bool>& events,
                                      const Key& id, bool handled) {
    const auto iterator = events.find(id);
    if (iterator == events.end()) {
        return std::nullopt;
    }
    const bool result = iterator->second;
    if (result && handled) {
        iterator->second = false;
    }
    return result;
}

}  // namespace ludork::engine::input_impl
