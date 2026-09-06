#include "GameplayValueUtils.hpp"
#include <Runtime/RuntimeReflection.hpp>

namespace ludork::global::gameplay_detail {

RuntimeIdentityPtr runtimeMap(RuntimeValue::Map values) {
    RuntimeIdentityPtr result = createRuntimeMapIdentity();
    for (auto& [name, value] : values) {
        runtimeReflection().set(
            ludork::runtime::reference::intern(RuntimeValue(result)), name,
            value);
    }
    return result;
}

}  // namespace ludork::global::gameplay_detail
