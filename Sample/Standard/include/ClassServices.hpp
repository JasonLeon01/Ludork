#pragma once

#include <StandardApi.hpp>

#include <sol2/forward.hpp>

#include <string>

struct lua_State;

namespace ludork::standard::class_runtime {

LUDORK_STANDARD_API sol::table finalizeClass(sol::table definition,
                                             const sol::table& bases);

LUDORK_STANDARD_API void registerNativeClass(sol::table nativeType,
                                             const sol::table& metadata);

LUDORK_STANDARD_API void registerNativeClassDefaultResolver(
    sol::state_view lua, const sol::protected_function& callback);

LUDORK_STANDARD_API void unregisterNativeClassDefaultResolver(
    sol::state_view lua);

LUDORK_STANDARD_API sol::object protectedGet(sol::state_view lua,
                                             const sol::object& target,
                                             const sol::object& key);

LUDORK_STANDARD_API void protectedSet(sol::state_view lua,
                                      const sol::object& target,
                                      const sol::object& key,
                                      const sol::object& value);

LUDORK_STANDARD_API sol::object rawGetOwnField(sol::state_view lua,
                                               const sol::object& target,
                                               const sol::object& key);

LUDORK_STANDARD_API bool hasOwnField(sol::state_view lua,
                                     const sol::object& target,
                                     const sol::object& key);

LUDORK_STANDARD_API sol::table getOwnKeys(sol::state_view lua,
                                          const sol::object& target);

LUDORK_STANDARD_API bool rawEqual(const sol::object& left,
                                  const sol::object& right);

LUDORK_STANDARD_API sol::table getMroCopy(sol::state_view lua,
                                          const sol::object& value);

LUDORK_STANDARD_API sol::object typeOf(sol::state_view lua,
                                       const sol::object& value);

LUDORK_STANDARD_API bool isInstanceOf(sol::state_view lua,
                                      const sol::object& value,
                                      const sol::table& targetClass);

LUDORK_STANDARD_API bool isSubclassOf(sol::state_view lua,
                                      const sol::table& value,
                                      const sol::table& targetClass);

LUDORK_STANDARD_API sol::object clonePlainData(sol::state_view lua,
                                               const sol::object& value);

LUDORK_STANDARD_API sol::object shallowCopy(sol::state_view lua,
                                            const sol::object& value);

LUDORK_STANDARD_API sol::object deepCopy(sol::state_view lua,
                                         const sol::object& value);

LUDORK_STANDARD_API sol::object requireModule(sol::state_view lua,
                                              const std::string& moduleName);

LUDORK_STANDARD_API int invoke(lua_State* state, const sol::object& callable,
                               int argumentCount);

}  // namespace ludork::standard::class_runtime
