#include <Runtime/RuntimeReference.hpp>
#include <Runtime/Blueprint/ClassRuntime.hpp>

#include "ClassRuntime/ClassRuntimeInternal.hpp"
#include <Runtime/RuntimeSession.hpp>

#include <RuntimeSession.hpp>

extern "C" {
#include <lua.h>
}

#include <utility>

using namespace ludork::runtime::class_runtime_detail;
using namespace ludork::runtime::reference;

void ludork::runtime::class_runtime_detail::initializeClassRuntime(
    lua_State* state) {
    if (state == nullptr) {
        return;
    }
    clearResolverState(state);
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
    clearResolverState(state);
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

std::shared_ptr<Graph> ClassRuntimeFacade::instantiateGraph(
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
    return resolverState().records.contains(classPath);
}

std::optional<std::string> ClassRuntimeFacade::findCachedPathByName(
    const std::string& className) const {
    ludork::runtime::RuntimeScope runtime;
    const auto& names = resolverState().classNames;
    const auto found = names.find(className);
    return found == names.end() ? std::nullopt
                                : std::optional<std::string>(found->second);
}

ClassRuntimeFacade& classRuntime() {
    static ClassRuntimeFacade runtime;
    return runtime;
}
