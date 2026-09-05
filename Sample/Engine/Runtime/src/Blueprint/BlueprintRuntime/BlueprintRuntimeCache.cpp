#include <Runtime/RuntimeReference.hpp>
#include "BlueprintRuntimeInternal.hpp"

namespace ludork::runtime::blueprint_detail {

using namespace ludork::runtime::reference;

std::function<RuntimeValue(const RuntimeValue&)>& objectGraphResolver() {
    static std::function<RuntimeValue(const RuntimeValue&)> resolver;
    return resolver;
}

RuntimeValue objectGraph(const RuntimeValue& object) {
    const auto resolver = objectGraphResolver();
    return resolver ? resolver(object) : RuntimeValue();
}

void clearBlueprintRuntimeCaches() {
    objectGraphResolver() = {};
    rawSet(registry(), BLUEPRINT_IMPLEMENTATION_CACHE_KEY, RuntimeValue());
    rawSet(registry(), BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY, RuntimeValue());
    rawSet(registry(), BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY, RuntimeValue());
}

}  // namespace ludork::runtime::blueprint_detail
