#include <Runtime/RuntimeReference.hpp>
#include "BlueprintRuntimeInternal.hpp"

namespace ludork::engine::runtime_detail {

using namespace ludork::runtime::reference;

void clearBlueprintRuntimeCaches() {
    rawSet(registry(), BLUEPRINT_IMPLEMENTATION_CACHE_KEY, RuntimeValue());
    rawSet(registry(), BLUEPRINT_EVENT_DESCRIPTOR_CACHE_KEY, RuntimeValue());
    rawSet(registry(), BLUEPRINT_CALLABLE_PARAMETER_CACHE_KEY, RuntimeValue());
}

}  // namespace ludork::engine::runtime_detail
