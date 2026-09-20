#pragma once

#include <Runtime/RuntimeReference.hpp>
#include "ClassRuntimeRecords.hpp"

#include <Runtime/RuntimeValue.hpp>

#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace ludork::runtime::class_runtime_detail {

void initializeClassRuntime(lua_State* state);
void shutdownClassRuntime(lua_State* state) noexcept;

inline constexpr const char* CLASS_RESOLVER_STATE_KEY =
    "Ludork.Runtime.classResolverState";

RuntimeHandle requireModuleTable(const std::string& moduleName);
ClassRuntimeState& resolverState();
ClassRuntimeState* existingResolverState(lua_State* state) noexcept;
void clearResolverState(lua_State* state) noexcept;
std::shared_ptr<Graph> compileGraphTemplate(const RuntimeValue& data,
                                            const RuntimeValue& classType);
bool classGraphHasExecutableEvent(const std::string& classPath,
                                  const std::string& eventName);
std::shared_ptr<Graph> instantiateClassGraph(const std::string& classPath,
                                             const RuntimeValue& parent);
std::string declaringModule(const RuntimeValue& value);
RuntimeValue cloneMetadataValue(const RuntimeValue& value,
                                const RuntimeValue& fieldMetadata,
                                const std::string& fallbackModule = {},
                                bool stored = true);
RuntimeValue cloneAttrValue(const RuntimeValue& parentClass,
                            const RuntimeValue& key, const RuntimeValue& value,
                            const RuntimeValue& rawMetadata,
                            const RuntimeValue& rawTargetType,
                            bool stored = true);
std::vector<ClassRuntimeState::ClassRecord::ConfigReference> configReferences(
    const RuntimeValue& owner);
ClassRuntimeState::ClassRecord::InstanceAttributePlan compileAttributePlan(
    const std::string& name, const RuntimeValue& value,
    const RuntimeValue& parentClass, const RuntimeValue& fieldMetadata,
    const RuntimeValue& targetType, bool copyOnly);
std::string normalizeScriptMixinPath(const std::string& value);
RuntimeValue loadScriptMixin(const std::string& classPath,
                             const std::string& scriptPath);
void validateScriptMixin(const RuntimeValue& mixin,
                         const std::string& classPath,
                         const std::string& scriptPath);
void validateScriptMixinMembers(const RuntimeValue& parentClass,
                                const RuntimeValue& mixin,
                                const std::string& classPath,
                                const std::string& scriptPath);
void mergeScriptMixin(const RuntimeValue& parentClass,
                      const RuntimeValue& mixin, RuntimeValue definition,
                      RuntimeValue instanceAttrs, const std::string& classPath,
                      const std::string& scriptPath);
void applyConfigValues(
    const RuntimeValue& parentClass, RuntimeValue classAttrs,
    const std::vector<ClassRuntimeState::ClassRecord::ConfigReference>&
        references);
void initializeGeneratedInstance(lua_State* state, const std::string& classPath,
                                 const RuntimeValue& self,
                                 const RuntimeValue::Array& arguments);
std::tuple<RuntimeValue, RuntimeValue> resolveClass(
    const RuntimeValue& rawPath, const RuntimeValue& rawRoot);

}  // namespace ludork::runtime::class_runtime_detail
