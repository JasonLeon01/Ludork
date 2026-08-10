#include "RuntimeSubsystemServices.hpp"

#include <stdexcept>

namespace ludork::engine::runtime_detail {

const std::vector<std::string>& blueprintRuntimeServiceNames() {
    static const std::vector<std::string> names{
        "blueprint.BlueprintEvent",
        "blueprint.HasBlueprintEvent",
        "blueprint._tryExecuteInfoGraph",
        "blueprint._classHasBlueprintEvent",
        "blueprint._graphHasExecutableEvent",
        "blueprint._graphDataHasExecutableEvent",
        "blueprint.ExecuteParentEvent",
        "blueprint._executeGraph",
        "blueprint.ExecuteInfoGraph",
        "blueprint._resolveGeneralDataDict",
        "blueprint.ApplyGeneralData",
        "blueprint.InitInfo",
        "blueprint.GetRegisteredEvents",
        "blueprint.GetInfoType",
    };
    return names;
}

ServiceDispatchResult dispatchBlueprintRuntimeService(
    sol::this_state state, const std::string& operation,
    const sol::table& arguments) {
    sol::state_view lua(state);
    const sol::object first = runtimeResolverArgument(lua, arguments, 1);
    const sol::object second = runtimeResolverArgument(lua, arguments, 2);
    if (operation == "blueprint.BlueprintEvent") {
        const sol::object rawEvent = runtimeResolverArgument(lua, arguments, 3);
        if (!rawEvent.is<std::string>()) {
            throw std::invalid_argument(
                "Blueprint event name must be a string");
        }
        dispatchBlueprintEvent(
            state, first, second, rawEvent.as<std::string>(),
            runtimeResolverArgument(lua, arguments, 4),
            completionCallback(runtimeResolverArgument(lua, arguments, 5)));
        return runtimeResolverResult(lua, {});
    }
    if (operation == "blueprint.HasBlueprintEvent") {
        const bool result =
            second.is<std::string>() &&
            hasBlueprintEvent(state, first, second.as<std::string>());
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "blueprint._tryExecuteInfoGraph") {
        const bool result =
            second.is<std::string>() &&
            tryExecuteInfoBlueprintGraph(
                state, first, second.as<std::string>(),
                runtimeResolverArgument(lua, arguments, 3),
                completionCallback(runtimeResolverArgument(lua, arguments, 4)));
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "blueprint._classHasBlueprintEvent") {
        const bool result =
            second.is<std::string>() &&
            classHasBlueprintEvent(state, first, second.as<std::string>());
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "blueprint._graphHasExecutableEvent") {
        const bool result = second.is<std::string>() &&
                            blueprintGraphHasExecutableEvent(
                                lua, first, second.as<std::string>());
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "blueprint._graphDataHasExecutableEvent") {
        const bool result = second.is<std::string>() &&
                            blueprintGraphDataHasExecutableEvent(
                                lua, first, second.as<std::string>());
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "blueprint.ExecuteParentEvent") {
        const sol::object rawEvent = runtimeResolverArgument(lua, arguments, 3);
        const bool result =
            rawEvent.is<std::string>() &&
            executeParentBlueprintEvent(
                state, first, second, rawEvent.as<std::string>(),
                runtimeResolverArgument(lua, arguments, 4),
                runtimeResolverArgument(lua, arguments, 5),
                runtimeResolverArgument(lua, arguments, 6),
                completionCallback(runtimeResolverArgument(lua, arguments, 7)));
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "blueprint._executeGraph") {
        const bool result =
            second.is<std::string>() &&
            executeBlueprintGraph(
                lua, first, second.as<std::string>(),
                runtimeResolverArgument(lua, arguments, 3),
                runtimeResolverArgument(lua, arguments, 4),
                completionCallback(runtimeResolverArgument(lua, arguments, 5)));
        return runtimeResolverResult(lua, {sol::make_object(lua, result)});
    }
    if (operation == "blueprint.ExecuteInfoGraph") {
        if (second.is<std::string>()) {
            tryExecuteInfoBlueprintGraph(
                state, first, second.as<std::string>(),
                runtimeResolverArgument(lua, arguments, 3), {});
        }
        return runtimeResolverResult(lua, {});
    }
    if (operation == "blueprint._resolveGeneralDataDict") {
        return runtimeResolverResult(
            lua, {resolveGeneralDataDictionary(lua, first)});
    }
    if (operation == "blueprint.ApplyGeneralData") {
        applyBlueprintGeneralData(lua, first, second,
                                  runtimeResolverArgument(lua, arguments, 3));
        return runtimeResolverResult(lua, {});
    }
    if (operation == "blueprint.InitInfo") {
        initializeBlueprintInfo(state, first, second);
        return runtimeResolverResult(lua, {});
    }
    if (operation == "blueprint.GetRegisteredEvents") {
        return runtimeResolverResult(
            lua,
            {sol::make_object(lua, registeredBlueprintEvents(lua, first))});
    }
    if (operation == "blueprint.GetInfoType") {
        const sol::object value =
            runtimeIndex(lua, first, sol::make_object(lua, "_infoType"), false);
        return runtimeResolverResult(
            lua,
            {value.is<std::string>() ? value
                                     : sol::make_object(lua, std::string())});
    }
    return std::nullopt;
}

}  // namespace ludork::engine::runtime_detail
