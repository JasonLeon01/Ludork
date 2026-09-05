#include <Runtime/RuntimeReference.hpp>
#include <Gameplay/EngineClassRuntime.hpp>

#include "EngineClassRuntimeInternal.hpp"
#include <Runtime/RuntimeSession.hpp>

#include <Runtime/RuntimeProviders.hpp>
#include <RuntimeSession.hpp>

#include <utility>

using namespace ludork::engine::class_runtime_detail;

namespace {

void invalidateClass(const std::string& path) {
    runtimeProviders().invalidateBlueprintClassData(path);
    RuntimeValue resolver = resolverState();
    const RuntimeValue rawRecord =
        rawGet(requireTable(rawGet(resolver, "records")), path);
    if (isTable(rawRecord)) {
        rawSet(rawRecord, "graphTemplate", RuntimeValue());
        rawSet(rawRecord, "graphCompiled", false);
    }
    rawSet(requireTable(rawGet(resolver, "classes")), path, RuntimeValue());
    rawSet(requireTable(rawGet(resolver, "classData")), path, RuntimeValue());
    rawSet(requireTable(rawGet(resolver, "records")), path, RuntimeValue());
}

}  // namespace

void initializeEngineClassRuntime(lua_State* state) {
    if (state == nullptr) {
        return;
    }
    rawSet(registry(), CLASS_RESOLVER_STATE_KEY, RuntimeValue());
    resolverState();
    setNativeDefaultResolver(
        [state](const RuntimeValue::Array& arguments) -> RuntimeValue::Array {
            ludork::standard::LuaExecutionScope execution(state);
            if (!execution.active()) {
                return {RuntimeValue()};
            }
            if (arguments.size() != 3) {
                throw std::invalid_argument(
                    "Native default resolver expects three arguments");
            }
            return {cloneMetadataValue(arguments[0], requireTable(arguments[1]),
                                       declaringModule(arguments[2]))};
        });
}

void shutdownEngineClassRuntime(lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    clearNativeDefaultResolver(state);
    rawSet(registry(), CLASS_RESOLVER_STATE_KEY, RuntimeValue());
}

EngineResolvedClass EngineClassRuntimeFacade::resolve(
    const std::string& classPath,
    const std::optional<std::string>& root) const {
    ludork::runtime::RuntimeScope runtime;
    const auto [classType, classDataValue] = resolveClass(
        retain(makeValue(classPath)),
        root.has_value() ? retain(makeValue(*root)) : RuntimeValue());
    return {data(classType), data(classDataValue)};
}

RuntimeValue EngineClassRuntimeFacade::classData(
    const std::string& classPath) const {
    ludork::runtime::RuntimeScope runtime;
    const RuntimeValue value =
        rawGet(requireTable(rawGet(resolverState(), "classData")), classPath);
    return data(!value.isNil() ? value : RuntimeValue());
}

RuntimeValue EngineClassRuntimeFacade::instantiateGraph(
    const std::string& classPath, const RuntimeValue& parent) const {
    ludork::runtime::RuntimeScope runtime;
    return data(instantiateClassGraph(classPath, retain(makeValue(parent))));
}

bool EngineClassRuntimeFacade::graphHasExecutableEvent(
    const std::string& classPath, const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return classGraphHasExecutableEvent(classPath, eventName);
}

void EngineClassRuntimeFacade::invalidate(const std::string& classPath) const {
    ludork::runtime::RuntimeScope runtime;
    invalidateClass(classPath);
}

EngineClassRuntimeFacade& engineClassRuntime() {
    static EngineClassRuntimeFacade runtime;
    return runtime;
}
