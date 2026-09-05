#include <Runtime/RuntimeReference.hpp>
#include <Runtime/Blueprint/ClassRuntime.hpp>

#include "ClassRuntime/ClassRuntimeInternal.hpp"
#include <Runtime/RuntimeSession.hpp>

#include <RuntimeSession.hpp>

#include <utility>

using namespace ludork::runtime::class_runtime_detail;

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
    const auto [classType, classDataValue] =
        resolveClass(RuntimeValue(classPath),
                     root.has_value() ? RuntimeValue(*root) : RuntimeValue());
    return {intern(classType), classDataValue};
}

RuntimeValue ClassRuntimeFacade::classData(const std::string& classPath) const {
    ludork::runtime::RuntimeScope runtime;
    return std::get<1>(resolveClass(RuntimeValue(classPath), RuntimeValue()));
}

RuntimeValue ClassRuntimeFacade::instantiateGraph(
    const std::string& classPath, const RuntimeValue& parent) const {
    ludork::runtime::RuntimeScope runtime;
    return instantiateClassGraph(classPath, parent);
}

bool ClassRuntimeFacade::graphHasExecutableEvent(
    const std::string& classPath, const std::string& eventName) const {
    ludork::runtime::RuntimeScope runtime;
    return classGraphHasExecutableEvent(classPath, eventName);
}

bool ClassRuntimeFacade::containsCached(const std::string& classPath) const {
    ludork::runtime::RuntimeScope runtime;
    return !rawGet(requireTable(rawGet(resolverState(), "classes")), classPath)
                .isNil();
}

std::optional<std::string> ClassRuntimeFacade::findCachedPathByName(
    const std::string& className) const {
    ludork::runtime::RuntimeScope runtime;
    const RuntimeValue path =
        rawGet(requireTable(rawGet(resolverState(), "classNames")), className);
    return path.isNil() ? std::nullopt
                        : std::optional<std::string>(as<std::string>(path));
}

ClassRuntimeFacade& classRuntime() {
    static ClassRuntimeFacade runtime;
    return runtime;
}
