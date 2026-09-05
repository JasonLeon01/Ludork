#include <Runtime/RuntimeProviders.hpp>

#include "RuntimeProviderInternals.hpp"
#include <Runtime/Detail/RuntimeServices.hpp>

#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <RuntimeSession.hpp>

#include <array>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace ludork::runtime::detail {
namespace {

constexpr std::size_t PROVIDER_COUNT = 6;

struct RuntimeProviderState {
    std::mutex mutex;
    std::array<RuntimeIdentityPtr, PROVIDER_COUNT> providers;
};

RuntimeProviderState& providerState() {
    static RuntimeProviderState state;
    return state;
}

std::size_t providerIndex(RuntimeProviderSlot slot) {
    return static_cast<std::size_t>(slot);
}

const char* providerName(RuntimeProviderSlot slot) {
    switch (slot) {
        case RuntimeProviderSlot::Curve:
            return "curve resolver";
        case RuntimeProviderSlot::PlainTextConfig:
            return "plain text config resolver";
        case RuntimeProviderSlot::BlueprintClassDataByPath:
            return "Blueprint class data resolver";
        case RuntimeProviderSlot::BlueprintCompileGraph:
            return "Blueprint graph compiler";
        case RuntimeProviderSlot::BlueprintInstantiateGraphTemplate:
            return "Blueprint graph template instantiator";
        case RuntimeProviderSlot::Config:
            return "config resolver";
    }
    throw std::logic_error("Unknown runtime provider slot");
}

sol::protected_function providerFunction(sol::state_view lua,
                                         RuntimeProviderSlot slot) {
    RuntimeIdentityPtr provider;
    {
        std::lock_guard<std::mutex> lock(providerState().mutex);
        provider = providerState().providers[providerIndex(slot)];
    }
    if (!provider) {
        throw std::runtime_error(std::string("Runtime ") + providerName(slot) +
                                 " is not installed");
    }
    const sol::object value =
        ludork::runtime::binding::writeOpaqueIdentity(lua, provider);
    if (!value.is<sol::protected_function>()) {
        throw std::runtime_error(std::string("Runtime ") + providerName(slot) +
                                 " is unavailable in the active Lua VM");
    }
    return value.as<sol::protected_function>();
}

void validateProvider(sol::state_view lua, RuntimeProviderSlot slot,
                      const RuntimeIdentityPtr& provider) {
    if (!provider) {
        throw std::invalid_argument(std::string("Runtime ") +
                                    providerName(slot) + " must not be nil");
    }
    const sol::object value =
        ludork::runtime::binding::writeOpaqueIdentity(lua, provider);
    if (!value.is<sol::protected_function>()) {
        throw std::invalid_argument(
            std::string("Runtime ") + providerName(slot) +
            " must be a function from the active Lua VM");
    }
}

void installProvider(sol::state_view lua, RuntimeProviderSlot slot,
                     const RuntimeIdentityPtr& provider) {
    validateProvider(lua, slot, provider);
    std::lock_guard<std::mutex> lock(providerState().mutex);
    RuntimeIdentityPtr& target = providerState().providers[providerIndex(slot)];
    if (target) {
        throw std::runtime_error(std::string("Runtime ") + providerName(slot) +
                                 " is already installed");
    }
    target = provider;
}

void installProviderGroup(
    sol::state_view lua,
    const std::vector<std::pair<RuntimeProviderSlot, RuntimeIdentityPtr>>&
        providers) {
    for (const auto& [slot, provider] : providers) {
        validateProvider(lua, slot, provider);
    }
    std::lock_guard<std::mutex> lock(providerState().mutex);
    for (const auto& [slot, provider] : providers) {
        static_cast<void>(provider);
        if (providerState().providers[providerIndex(slot)]) {
            throw std::runtime_error(std::string("Runtime ") +
                                     providerName(slot) +
                                     " is already installed");
        }
    }
    for (const auto& [slot, provider] : providers) {
        providerState().providers[providerIndex(slot)] = provider;
    }
}

int invokeProvider(sol::state_view lua, RuntimeProviderSlot slot,
                   const std::vector<sol::object>& arguments) {
    return invokeRuntimeFunction(lua.lua_state(), providerFunction(lua, slot),
                                 arguments, providerName(slot));
}

}  // namespace

sol::object invokeRuntimeProviderOne(
    sol::state_view lua, RuntimeProviderSlot slot,
    const std::vector<sol::object>& arguments) {
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        const int resultCount = invokeProvider(lua, slot, arguments);
        if (resultCount != 1) {
            throw std::runtime_error(std::string("Runtime ") +
                                     providerName(slot) +
                                     " must return exactly one value, got " +
                                     std::to_string(resultCount));
        }
        sol::object result = sol::stack::get<sol::object>(state, stackBase + 1);
        lua_settop(state, stackBase);
        return result;
    } catch (...) {
        lua_settop(state, stackBase);
        throw;
    }
}

void clearRuntimeProviders() noexcept {
    std::lock_guard<std::mutex> lock(providerState().mutex);
    providerState().providers = {};
}

void installDataRuntimeProviders(
    const RuntimeIdentityPtr& curveResolver,
    const RuntimeIdentityPtr& plainTextConfigResolver) {
    RuntimeScope runtime;
    installProviderGroup(
        sol::state_view(runtime.state()),
        {{RuntimeProviderSlot::Curve, curveResolver},
         {RuntimeProviderSlot::PlainTextConfig, plainTextConfigResolver}});
}

void installBlueprintRuntimeProviders(
    const RuntimeIdentityPtr& classDataByPath,
    const RuntimeIdentityPtr& compileGraph,
    const RuntimeIdentityPtr& instantiateGraphTemplate) {
    RuntimeScope runtime;
    installProviderGroup(
        sol::state_view(runtime.state()),
        {{RuntimeProviderSlot::BlueprintClassDataByPath, classDataByPath},
         {RuntimeProviderSlot::BlueprintCompileGraph, compileGraph},
         {RuntimeProviderSlot::BlueprintInstantiateGraphTemplate,
          instantiateGraphTemplate}});
}

void installConfigRuntimeProvider(const RuntimeIdentityPtr& configResolver) {
    RuntimeScope runtime;
    installProvider(sol::state_view(runtime.state()),
                    RuntimeProviderSlot::Config, configResolver);
}

}  // namespace ludork::runtime::detail

void RuntimeProviders::installData(
    const RuntimeIdentityPtr& curveResolver,
    const RuntimeIdentityPtr& plainTextConfigResolver) {
    ludork::runtime::detail::installDataRuntimeProviders(
        curveResolver, plainTextConfigResolver);
}

void RuntimeProviders::installBlueprint(
    const RuntimeIdentityPtr& classDataByPath,
    const RuntimeIdentityPtr& compileGraph,
    const RuntimeIdentityPtr& instantiateGraphTemplate) {
    ludork::runtime::detail::installBlueprintRuntimeProviders(
        classDataByPath, compileGraph, instantiateGraphTemplate);
}

void RuntimeProviders::installConfig(const RuntimeIdentityPtr& configResolver) {
    ludork::runtime::detail::installConfigRuntimeProvider(configResolver);
}

namespace {

RuntimeIdentityPtr invokeIdentityProvider(
    ludork::runtime::detail::RuntimeProviderSlot slot,
    const std::vector<RuntimeIdentityPtr>& identityArguments,
    const std::vector<std::string>& stringArguments = {}) {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    std::vector<sol::object> arguments;
    arguments.reserve(identityArguments.size() + stringArguments.size());
    for (const RuntimeIdentityPtr& argument : identityArguments) {
        arguments.push_back(
            ludork::runtime::binding::writeOpaqueIdentity(lua, argument));
    }
    for (const std::string& argument : stringArguments) {
        arguments.push_back(sol::make_object(lua, argument));
    }
    return ludork::runtime::binding::readOpaqueIdentity<RuntimeIdentityPtr>(
        ludork::runtime::detail::invokeRuntimeProviderOne(lua, slot,
                                                          arguments));
}

}  // namespace

RuntimeIdentityPtr RuntimeProviderFacade::curve(const std::string& name) const {
    return invokeIdentityProvider(
        ludork::runtime::detail::RuntimeProviderSlot::Curve, {}, {name});
}

RuntimeIdentityPtr RuntimeProviderFacade::plainTextConfig(
    const std::string& name) const {
    return invokeIdentityProvider(
        ludork::runtime::detail::RuntimeProviderSlot::PlainTextConfig, {},
        {name});
}

RuntimeIdentityPtr RuntimeProviderFacade::blueprintClassData(
    const std::string& classPath) const {
    return invokeIdentityProvider(
        ludork::runtime::detail::RuntimeProviderSlot::BlueprintClassDataByPath,
        {}, {classPath});
}

RuntimeIdentityPtr RuntimeProviderFacade::compileBlueprintGraph(
    const RuntimeIdentityPtr& graphData,
    const RuntimeIdentityPtr& classType) const {
    return invokeIdentityProvider(
        ludork::runtime::detail::RuntimeProviderSlot::BlueprintCompileGraph,
        {graphData, classType});
}

RuntimeIdentityPtr RuntimeProviderFacade::instantiateBlueprintGraph(
    const RuntimeIdentityPtr& graphTemplate,
    const RuntimeIdentityPtr& parent) const {
    return invokeIdentityProvider(ludork::runtime::detail::RuntimeProviderSlot::
                                      BlueprintInstantiateGraphTemplate,
                                  {graphTemplate, parent});
}

std::string RuntimeProviderFacade::config(
    const std::string& configName, const std::string& settingName) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = sol::state_view(runtime.state());
    const sol::object value = ludork::runtime::detail::invokeRuntimeProviderOne(
        lua, ludork::runtime::detail::RuntimeProviderSlot::Config,
        {sol::make_object(lua, configName),
         sol::make_object(lua, settingName)});
    if (!value.is<std::string>()) {
        throw std::runtime_error(
            "Runtime config resolver must return a string");
    }
    return value.as<std::string>();
}

RuntimeProviderFacade& runtimeProviders() {
    static RuntimeProviderFacade providers;
    return providers;
}
