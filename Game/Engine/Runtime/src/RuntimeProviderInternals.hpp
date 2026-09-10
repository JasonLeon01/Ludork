#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <sol2/sol.hpp>

#include <array>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace ludork::runtime::detail {

enum class RuntimeProviderSlot {
    Curve,
    PlainTextConfig,
    BlueprintClassDataByPath,
    BlueprintCompileGraph,
    BlueprintInstantiateGraphTemplate,
    Config,
};

constexpr std::size_t PROVIDER_COUNT = 6;

struct RuntimeProviderState {
    std::mutex mutex;
    std::array<RuntimeIdentityPtr, PROVIDER_COUNT> providers;
};

void installDataRuntimeProviders(
    const RuntimeIdentityPtr& curveResolver,
    const RuntimeIdentityPtr& plainTextConfigResolver);
void installBlueprintRuntimeProviders(
    const RuntimeIdentityPtr& classDataByPath,
    const RuntimeIdentityPtr& compileGraph,
    const RuntimeIdentityPtr& instantiateGraphTemplate);
void installConfigRuntimeProvider(const RuntimeIdentityPtr& configResolver);

sol::object invokeRuntimeProviderOne(
    sol::state_view lua, RuntimeProviderSlot slot,
    const std::vector<sol::object>& arguments = {});

}  // namespace ludork::runtime::detail
