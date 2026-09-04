#include <Gameplay/EngineClassRuntime.hpp>

#include "EngineClassRuntimeInternal.hpp"
#include <Runtime/RuntimeSession.hpp>

#include <ClassServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <Runtime/RuntimeProviders.hpp>
#include <RuntimeSession.hpp>

#include <sol2/sol.hpp>

#include <utility>

using namespace ludork::engine::class_runtime_detail;

namespace {

void invalidateClass(sol::state_view lua, const std::string& path) {
    runtimeProviders().invalidateBlueprintClassData(path);
    sol::table resolver = resolverState(lua);
    const sol::object rawRecord =
        resolver.raw_get<sol::table>("records").raw_get<sol::object>(path);
    if (rawRecord.is<sol::table>()) {
        rawRecord.as<sol::table>().raw_set("graphTemplate", sol::lua_nil);
        rawRecord.as<sol::table>().raw_set("graphCompiled", false);
    }
    resolver.raw_get<sol::table>("classes").raw_set(path, sol::lua_nil);
    resolver.raw_get<sol::table>("classData").raw_set(path, sol::lua_nil);
    resolver.raw_get<sol::table>("records").raw_set(path, sol::lua_nil);
}

}  // namespace

void initializeEngineClassRuntime(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    sol::state_view lua(state);
    lua.registry().raw_set(CLASS_RESOLVER_STATE_KEY, sol::lua_nil);
    resolverState(lua);
    const sol::object rawDefaultResolver = sol::make_object(
        lua,
        sol::as_function([state](sol::object value, sol::table fieldMetadata,
                                 sol::object rawModule) {
            ludork::standard::LuaExecutionScope execution(state);
            sol::state_view callbackLua(state);
            if (!execution.active()) {
                return nilObject(callbackLua);
            }
            return cloneMetadataValue(callbackLua, value, fieldMetadata,
                                      declaringModule(rawModule));
        }));
    ludork::standard::class_runtime::registerNativeClassDefaultResolver(
        lua, rawDefaultResolver.as<sol::protected_function>());
}

void shutdownEngineClassRuntime(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    sol::state_view lua(state);
    ludork::standard::class_runtime::unregisterNativeClassDefaultResolver(lua);
    lua.registry().raw_set(CLASS_RESOLVER_STATE_KEY, sol::lua_nil);
}

EngineResolvedClass EngineClassRuntimeFacade::resolve(
    const std::string& classPath,
    const std::optional<std::string>& root) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    const auto [classType, classDataValue] = resolveClass(
        lua, sol::make_object(lua, classPath),
        root.has_value() ? sol::make_object(lua, *root) : nilObject(lua));
    return {runtimeValue(classType), runtimeValue(classDataValue)};
}

RuntimeValue EngineClassRuntimeFacade::classData(
    const std::string& classPath) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    const sol::object value = resolverState(lua)
                                  .raw_get<sol::table>("classData")
                                  .raw_get<sol::object>(classPath);
    return runtimeValue(value.valid() ? value : nilObject(lua));
}

RuntimeValue EngineClassRuntimeFacade::instantiateGraph(
    const std::string& classPath, const RuntimeValue& parent) const {
    ludork::runtime::RuntimeScope runtime;
    sol::state_view lua = runtime.lua();
    return runtimeValue(instantiateClassGraph(
        lua, classPath, ludork::runtime::binding::writeLuaValue(lua, parent)));
}

bool EngineClassRuntimeFacade::graphHasExecutableEvent(
    const std::string& classPath, const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return classGraphHasExecutableEvent(runtime.lua(), classPath, eventName);
}

void EngineClassRuntimeFacade::invalidate(const std::string& classPath) const {
    ludork::runtime::RuntimeScope runtime;
    invalidateClass(runtime.lua(), classPath);
}

EngineClassRuntimeFacade& engineClassRuntime() {
    static EngineClassRuntimeFacade runtime;
    return runtime;
}
