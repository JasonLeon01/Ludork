#include <Runtime/RuntimeValueServices.hpp>

#include "RuntimeSubsystemServices.hpp"

namespace ludork::engine::runtime_detail {

const std::vector<std::string>& runtimeValueServiceNames() {
    static const std::vector<std::string> names{
        "components.cacheGet",
        "components.cacheSet",
    };
    return names;
}

ServiceDispatchResult dispatchRuntimeValueService(
    sol::this_state state, const std::string& operation,
    const RuntimeArguments& arguments) {
    sol::state_view lua(state);
    const sol::object first = runtimeResolverArgument(lua, arguments, 1);
    const sol::object second = runtimeResolverArgument(lua, arguments, 2);
    if (operation == "components.cacheGet") {
        const sol::object cached =
            componentCache(lua, first).raw_get<sol::object>(second);
        return runtimeResolverResult(
            lua, {cached.valid() ? cached : nilObject(lua)});
    }
    if (operation == "components.cacheSet") {
        componentCache(lua, first)
            .raw_set(second, runtimeResolverArgument(lua, arguments, 3));
        return runtimeResolverResult(lua, {});
    }
    return std::nullopt;
}

}  // namespace ludork::engine::runtime_detail
