#pragma once

#include <sol2/sol.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ludork::engine::runtime_detail {

struct RuntimeArguments {
    RuntimeArguments(lua_State* value, int firstIndex, int valueCount)
        : state(value),
          first(valueCount == 0 ? 0 : lua_absindex(value, firstIndex)),
          count(valueCount) {}

    [[nodiscard]] std::size_t size() const noexcept {
        return static_cast<std::size_t>(count);
    }

    lua_State* state = nullptr;
    int first = 0;
    int count = 0;
};

using ServiceDispatchResult = std::optional<int>;

void ensureRuntimeLuaStack(lua_State* state, std::size_t count,
                           const char* context);
int invokeRuntimeFunction(lua_State* state, const sol::object& callable,
                          const std::vector<sol::object>& arguments,
                          const char* context);
sol::object nilObject(sol::state_view lua);
std::function<void()> completionCallback(const sol::object& value);
sol::table registryTable(sol::state_view lua, const char* key,
                         const char* weakMode = nullptr);
sol::table componentCache(sol::state_view lua, const sol::object& rawKind);
sol::table objectMetatable(sol::state_view lua, const sol::object& value);
sol::object classType(sol::this_state state, const sol::object& value);
bool hasBlueprintEvent(sol::this_state state, const sol::object& object,
                       const std::string& eventName);
void dispatchBlueprintEvent(sol::this_state state, const sol::object& object,
                            const sol::object& rawObjectType,
                            const std::string& eventName,
                            const sol::object& rawKeywordArguments,
                            const std::function<void()>& onComplete);
void validateBlueprintEvent(sol::this_state state, const sol::object& object,
                            const std::string& eventName);
int runtimeResolverResult(sol::state_view lua,
                          const std::vector<sol::object>& values);
sol::object runtimeResolverArgument(sol::state_view lua,
                                    const RuntimeArguments& arguments,
                                    std::size_t index);
sol::object findRuntimeClassModule(sol::state_view lua,
                                   const sol::object& classReference);
sol::object syntheticRuntimeMetadata(sol::state_view lua,
                                     const sol::table& classTable);
sol::object runtimeTypeMetadata(sol::state_view lua,
                                const sol::table& classType);
sol::table collectRuntimeAttrMetadata(sol::state_view lua,
                                      const sol::table& owner);
std::pair<sol::object, sol::object> resolveRuntimeConfigVar(
    sol::state_view lua, const sol::object& owner, const sol::object& rawName);
std::pair<sol::object, sol::object> resolveRuntimeMemberMetadata(
    sol::state_view lua, const sol::object& owner, const sol::object& rawName);
sol::object evaluateRuntimeExpression(sol::state_view lua,
                                      const sol::object& rawExpression,
                                      const sol::object& rawEnvironment);
sol::object resolveRuntimeMetadataType(sol::state_view lua,
                                       const sol::object& typeReference,
                                       const sol::object& declaringModule);
bool runtimeSequence(const sol::table& table, std::vector<sol::object>& values);
sol::object runtimeIndex(sol::state_view lua, const sol::object& target,
                         const sol::object& key, bool raw);

bool blueprintGraphHasExecutableEvent(sol::state_view lua,
                                      const sol::object& graph,
                                      const std::string& eventName);
bool blueprintGraphDataHasExecutableEvent(sol::state_view lua,
                                          const sol::object& graphData,
                                          const std::string& eventName);
bool tryExecuteInfoBlueprintGraph(sol::this_state state,
                                  const sol::object& object,
                                  const std::string& eventName,
                                  const sol::object& rawKeywordArguments,
                                  const std::function<void()>& onComplete);
bool classHasBlueprintEvent(sol::this_state state, const sol::object& rawClass,
                            const std::string& eventName);
bool executeParentBlueprintEvent(sol::this_state state,
                                 const sol::object& object,
                                 const sol::object& rawObjectClass,
                                 const std::string& eventName,
                                 const sol::object& rawParentClass,
                                 const sol::object& rawKeywordArguments,
                                 const sol::object& rawImplementationOwner,
                                 const std::function<void()>& onComplete);
bool executeBlueprintGraph(sol::state_view lua, const sol::object& graph,
                           const std::string& eventName,
                           const sol::object& rawKeywordArguments,
                           const sol::object& graphClass,
                           const std::function<void()>& onComplete);
sol::object resolveGeneralDataDictionary(sol::state_view lua,
                                         const sol::object& value);
void applyBlueprintGeneralData(sol::state_view lua, const sol::object& object,
                               const sol::object& rawData,
                               const sol::object& rawParameterTypes);
void initializeBlueprintInfo(sol::this_state state, const sol::object& object,
                             const sol::object& dataProvider);
sol::table registeredBlueprintEvents(sol::state_view lua,
                                     const sol::object& rawClass);

int nodeGraphContext(sol::state_view lua, const RuntimeArguments& arguments);
int invokeNodeGraphCallable(sol::state_view lua,
                            const RuntimeArguments& arguments);
int createNodeGraphNode(sol::state_view lua,
                        const RuntimeArguments& arguments);
int bridgeNodeGraphCache(sol::state_view lua,
                         const RuntimeArguments& arguments);
int evaluateNodeGraphCondition(sol::state_view lua,
                               const RuntimeArguments& arguments);
void clearNodeGraphRuntimeCaches(sol::state_view lua);

const std::vector<std::string>& runtimeValueServiceNames();
ServiceDispatchResult dispatchRuntimeValueService(sol::this_state state,
                                                  const std::string& operation,
                                                  const RuntimeArguments& arguments);

const std::vector<std::string>& metadataRuntimeServiceNames();
ServiceDispatchResult dispatchMetadataRuntimeService(
    sol::this_state state, const std::string& operation,
    const RuntimeArguments& arguments);

const std::vector<std::string>& blueprintRuntimeServiceNames();
ServiceDispatchResult dispatchBlueprintRuntimeService(
    sol::this_state state, const std::string& operation,
    const RuntimeArguments& arguments);

const std::vector<std::string>& nodeGraphRuntimeServiceNames();
ServiceDispatchResult dispatchNodeGraphRuntimeService(
    sol::this_state state, const std::string& operation,
    const RuntimeArguments& arguments);

void clearRuntimeServiceCaches(sol::state_view lua);

}  // namespace ludork::engine::runtime_detail
