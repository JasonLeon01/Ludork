#include "Internal.hpp"

#include <Runtime/NodeGraphRuntime.hpp>
#include <Runtime/RuntimeValueServices.hpp>

#include <cmath>
#include <cstdint>
#include <limits>

namespace ludork::engine::graph_detail {

RuntimeIdentityPtr identityValue(const RuntimeValue* value) {
    if (value == nullptr) {
        return nullptr;
    }
    const RuntimeIdentityPtr* identity = value->getIf<RuntimeIdentityPtr>();
    return identity == nullptr ? nullptr : *identity;
}

std::optional<int> integerValue(const RuntimeValue& value) {
    if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
        if (*integer < std::numeric_limits<int>::min() ||
            *integer > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
        return static_cast<int>(*integer);
    }
    if (const double* number = value.getIf<double>()) {
        if (!std::isfinite(*number) || std::floor(*number) != *number ||
            *number < std::numeric_limits<int>::min() ||
            *number > std::numeric_limits<int>::max()) {
            return std::nullopt;
        }
        return static_cast<int>(*number);
    }
    return std::nullopt;
}

std::optional<NodeIndex> nodeIndexValue(const RuntimeValue& value) {
    if (const std::optional<int> index = integerValue(value)) {
        return NodeIndex(*index);
    }
    if (const std::string* name = value.getIf<std::string>()) {
        return NodeIndex(*name);
    }
    return std::nullopt;
}

RuntimeValue runtimeMember(const RuntimeValue& value, const std::string& name) {
    return ludork::engine::runtime_services::invokeFirst(
        "reflect.get", {value, RuntimeValue(name)});
}

RuntimeIdentityPtr callableWithin(const RuntimeValue& value,
                                  const std::string& path) {
    RuntimeValue current = value;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t separator = path.find('.', start);
        const std::size_t end =
            separator == std::string::npos ? path.size() : separator;
        if (end > start) {
            current = runtimeMember(current, path.substr(start, end - start));
        }
        if (current.isNil()) {
            return nullptr;
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    return identityValue(&current);
}

NodeCache decodeNodeCache(const RuntimeIdentityPtr& cacheIdentity) {
    return cacheIdentity == nullptr
               ? NodeCache{}
               : nodeGraphRuntime().readCache(cacheIdentity);
}

void syncNodeCache(const RuntimeIdentityPtr& cacheIdentity,
                   const NodeCache& cache) {
    if (cacheIdentity != nullptr) {
        nodeGraphRuntime().writeCache(cacheIdentity, cache);
    }
}

}  // namespace ludork::engine::graph_detail
