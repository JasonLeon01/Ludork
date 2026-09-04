#pragma once

#include <sol2/sol.hpp>

#include <string>
#include <vector>

namespace ludork::runtime::detail {

enum class RuntimeProviderSlot {
    Curve,
    PlainTextConfig,
    BlueprintClassDataByPath,
    BlueprintInvalidateClassData,
    BlueprintCompileGraph,
    BlueprintInstantiateGraphTemplate,
    Config,
};

sol::object invokeRuntimeProviderOne(
    sol::state_view lua, RuntimeProviderSlot slot,
    const std::vector<sol::object>& arguments = {});
void invokeRuntimeProviderVoid(sol::state_view lua, RuntimeProviderSlot slot,
                               const std::vector<sol::object>& arguments = {});

}  // namespace ludork::runtime::detail
