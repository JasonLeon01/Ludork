#pragma once

#include <StandardApi.hpp>

#include <string>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>

struct lua_State;

namespace ludork::standard {

class LUDORK_STANDARD_API LuaScriptError final : public std::runtime_error {
public:
    explicit LuaScriptError(const std::string& message);
};

LUDORK_STANDARD_API void installLuaErrorHandler(lua_State* state);
LUDORK_STANDARD_API int protectedLuaCall(lua_State* state, int argumentCount,
                                         int resultCount);
LUDORK_STANDARD_API std::string luaErrorMessage(lua_State* state, int index);
[[noreturn]] LUDORK_STANDARD_API void throwLuaCallError(lua_State* state,
                                                        int status);
LUDORK_STANDARD_API std::string luaThreadTraceback(lua_State* state,
                                                   lua_State* thread,
                                                   const std::string& message);
LUDORK_STANDARD_API void logLuaCallbackError(std::string_view name,
                                             const LuaScriptError& error);

template <typename Callback>
bool dispatchLuaCallback(std::string_view name, const Callback& callback) {
#if LUDORK_WITH_LUA
    try {
        callback();
    } catch (const LuaScriptError& error) {
        logLuaCallbackError(name, error);
        return false;
    }
#else
    static_cast<void>(name);
    callback();
#endif
    return true;
}

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
