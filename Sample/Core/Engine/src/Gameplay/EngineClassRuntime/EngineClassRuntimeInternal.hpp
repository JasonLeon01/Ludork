#pragma once

#include <Runtime/RuntimeReference.hpp>

#include <Runtime/RuntimeValue.hpp>

#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace ludork::engine::class_runtime_detail {

using namespace ludork::runtime::reference;

inline constexpr const char* CLASS_RESOLVER_STATE_KEY =
    "Ludork.Engine.classResolverState";

RuntimeValue requireModuleTable(const std::string& moduleName);
RuntimeValue resolverState();
RuntimeValue compileGraphTemplate(const RuntimeValue& data,
                                  const RuntimeValue& classType);
bool classGraphHasExecutableEvent(const std::string& classPath,
                                  const std::string& eventName);
RuntimeValue instantiateClassGraph(const std::string& classPath,
                                   const RuntimeValue& parent);
std::string declaringModule(const RuntimeValue& value);
RuntimeValue cloneMetadataValue(const RuntimeValue& value,
                                const RuntimeValue& fieldMetadata,
                                const std::string& fallbackModule = {});
RuntimeValue cloneAttrValue(const RuntimeValue& parentClass,
                            const RuntimeValue& key, const RuntimeValue& value,
                            const RuntimeValue& rawMetadata,
                            const RuntimeValue& rawTargetType);
RuntimeValue configReferences(const RuntimeValue& owner);
std::string normalizeScriptMixinPath(const std::string& value);
RuntimeValue loadScriptMixin(const std::string& classPath,
                             const std::string& scriptPath);
void mergeScriptMixin(const RuntimeValue& parentClass,
                      const RuntimeValue& mixin, RuntimeValue definition,
                      RuntimeValue instanceAttrs, const std::string& classPath,
                      const std::string& scriptPath);
void applyConfigValues(const RuntimeValue& parentClass, RuntimeValue classAttrs,
                       const RuntimeValue& references);
void initializeGeneratedInstance(lua_State* state, const std::string& classPath,
                                 const RuntimeValue& self,
                                 const RuntimeValue::Array& arguments);
std::tuple<RuntimeValue, RuntimeValue> resolveClass(
    const RuntimeValue& rawPath, const RuntimeValue& rawRoot);

}  // namespace ludork::engine::class_runtime_detail
