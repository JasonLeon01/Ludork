#include <Runtime/RuntimeReference.hpp>
#include <Runtime/Blueprint/ClassRuntime.hpp>

#include "ClassRuntime/ClassRuntimeInternal.hpp"
#include <Runtime/RuntimeSession.hpp>

#include <Runtime/RuntimeProviders.hpp>
#include <RuntimeSession.hpp>

#include <utility>

using namespace ludork::runtime::class_runtime_detail;

namespace {

void invalidateClass(const std::string& path) {
    runtimeProviders().invalidateBlueprintClassData(path);
    RuntimeHandle resolver = resolverState();
    const RuntimeValue rawRecord =
        rawGet(requireTable(rawGet(resolver, "records")), path);
    if (isTable(rawRecord)) {
        rawSet(ludork::runtime::reference::intern(rawRecord), "graphTemplate",
               RuntimeValue());
        rawSet(ludork::runtime::reference::intern(rawRecord), "graphCompiled",
               false);
    }
    rawSet(requireTable(rawGet(resolver, "classes")), path, RuntimeValue());
    rawSet(requireTable(rawGet(resolver, "classData")), path, RuntimeValue());
    rawSet(requireTable(rawGet(resolver, "records")), path, RuntimeValue());
}

}  // namespace

void ludork::runtime::class_runtime_detail::initializeClassRuntime(
    lua_State* state) {
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

void ludork::runtime::class_runtime_detail::shutdownClassRuntime(
    lua_State* state) noexcept {
    if (state == nullptr) {
        return;
    }
    clearNativeDefaultResolver(state);
    rawSet(registry(), CLASS_RESOLVER_STATE_KEY, RuntimeValue());
}

ResolvedClass ClassRuntimeFacade::resolve(
    const std::string& classPath,
    const std::optional<std::string>& root) const {
    ludork::runtime::RuntimeScope runtime;
    const auto [classType, classDataValue] = resolveClass(
        retain(makeValue(classPath)),
        root.has_value() ? retain(makeValue(*root)) : RuntimeValue());
    return {intern(classType), snapshot(classDataValue)};
}

RuntimeValue ClassRuntimeFacade::classData(const std::string& classPath) const {
    ludork::runtime::RuntimeScope runtime;
    const RuntimeValue value = rawGet(
        requireTable(rawGet(ludork::runtime::reference::intern(resolverState()),
                            "classData")),
        classPath);
    return snapshot(!value.isNil() ? value : RuntimeValue());
}

RuntimeValue ClassRuntimeFacade::instantiateGraph(
    const std::string& classPath, const RuntimeValue& parent) const {
    ludork::runtime::RuntimeScope runtime;
    return snapshot(
        instantiateClassGraph(classPath, retain(makeValue(parent))));
}

bool ClassRuntimeFacade::graphHasExecutableEvent(
    const std::string& classPath, const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return classGraphHasExecutableEvent(classPath, eventName);
}

void ClassRuntimeFacade::invalidate(const std::string& classPath) const {
    ludork::runtime::RuntimeScope runtime;
    invalidateClass(classPath);
}

ClassRuntimeFacade& classRuntime() {
    static ClassRuntimeFacade runtime;
    return runtime;
}
