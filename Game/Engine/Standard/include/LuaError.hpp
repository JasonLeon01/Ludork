#pragma once

#include <StandardApi.hpp>

#include <string>
#include <memory>
#include <type_traits>

struct lua_State;

namespace ludork::standard {

LUDORK_STANDARD_API void installLuaErrorHandler(lua_State* state);
LUDORK_STANDARD_API int protectedLuaCall(lua_State* state, int argumentCount,
                                         int resultCount);
LUDORK_STANDARD_API std::string luaErrorMessage(lua_State* state, int index);
LUDORK_STANDARD_API bool compareLuaValues(lua_State* state, int leftIndex,
                                          int rightIndex, int operation);

LUDORK_STANDARD_API int invokeLuaCallback(lua_State* state, const void* context,
                                          int (*callback)(const void*));

template <typename Callback>
int protectedLuaCallback(lua_State* state, const Callback& callback) {
    static_assert(std::is_trivially_destructible_v<Callback>);
    return invokeLuaCallback(
        state, std::addressof(callback), [](const void* context) {
            return (*static_cast<const Callback*>(context))();
        });
}

}  // namespace ludork::standard
