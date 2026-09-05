#pragma once

#include <Runtime/NodeGraph/Types.hpp>

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ludork::runtime::graph_detail {

RuntimeIdentityPtr identityValue(const RuntimeValue* value);
std::optional<int> integerValue(const RuntimeValue& value);
std::optional<NodeIndex> nodeIndexValue(const RuntimeValue& value);
RuntimeValue runtimeMember(const RuntimeValue& value, const std::string& name);
RuntimeIdentityPtr callableWithin(const RuntimeValue& value,
                                  const std::string& path);
template <typename Value>
std::vector<int> sortedPins(const std::unordered_map<int, Value>& pins) {
    std::vector<int> result;
    result.reserve(pins.size());
    for (const auto& [pin, _] : pins) {
        result.push_back(pin);
    }
    std::sort(result.begin(), result.end());
    return result;
}
NodeCache decodeNodeCache(const RuntimeIdentityPtr& cacheIdentity);
void syncNodeCache(const RuntimeIdentityPtr& cacheIdentity,
                   const NodeCache& cache);

}  // namespace ludork::runtime::graph_detail
