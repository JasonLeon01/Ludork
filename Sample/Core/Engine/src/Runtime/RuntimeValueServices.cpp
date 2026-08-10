#include <Runtime/RuntimeValueServices.hpp>

#include "RuntimeSubsystemServices.hpp"

#include <utility>

namespace ludork::engine::runtime_services {

RuntimeValue firstResult(const std::vector<RuntimeValue>& values) {
    return values.empty() ? RuntimeValue() : values.front();
}

RuntimeValue invokeFirst(const std::string& operation,
                         std::vector<RuntimeValue> arguments) {
    return firstResult(resolveRuntime(operation, arguments));
}

bool invokeBool(const std::string& operation,
                std::vector<RuntimeValue> arguments) {
    const RuntimeValue value = invokeFirst(operation, std::move(arguments));
    const bool* result = value.getIf<bool>();
    return result != nullptr && *result;
}

std::string invokeString(const std::string& operation,
                         std::vector<RuntimeValue> arguments) {
    const RuntimeValue value = invokeFirst(operation, std::move(arguments));
    const std::string* result = value.getIf<std::string>();
    return result == nullptr ? std::string() : *result;
}

}  // namespace ludork::engine::runtime_services

namespace ludork::engine::runtime_detail {

const std::vector<std::string>& runtimeValueServiceNames() {
    static const std::vector<std::string> names{
        "components.cacheGet",
        "components.cacheSet",
    };
    return names;
}

ServiceDispatchResult dispatchRuntimeValueService(sol::this_state state,
                                                  const std::string& operation,
                                                  const sol::table& arguments) {
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
