#pragma once

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lua.h>
}

#include <string>
#include <vector>

struct lua_State;

namespace ludork::standard::class_runtime::detail {

struct CallableInfo {
    int parameterCount = 0;
    bool vararg = false;
    std::vector<std::string> parameterNames;
};

CallableInfo inspectCallable(const lua_glue::Object& callable);
lua_glue::Table constructorClass(lua_State* state);
void setClassClosure(lua_State* state, const lua_glue::Table& target,
                     const char* name, const lua_glue::Table& classTable,
                     lua_CFunction function);
lua_glue::Table finalizeClassImpl(lua_glue::Table definition,
                                  const lua_glue::Table& bases);
lua_glue::Table ownFields(lua_glue::StateView lua,
                          const lua_glue::Object& target);
lua_glue::Object rawOwnField(lua_glue::StateView lua,
                             const lua_glue::Object& target,
                             const lua_glue::Object& key);
bool hasRawOwnField(lua_glue::StateView lua, const lua_glue::Object& target,
                    const lua_glue::Object& key);
lua_glue::Table ownKeyList(lua_glue::StateView lua,
                           const lua_glue::Object& target);
lua_glue::Table mroCopy(lua_glue::StateView lua, const lua_glue::Object& value);
int classNew(lua_State* state);
int classCall(lua_State* state);

lua_glue::Object allocateInstance(
    lua_glue::StateView lua, const lua_glue::Table& classTable,
    const lua_glue::Object& constructorArguments = lua_glue::Object(),
    bool allowDeferredRoots = false);
void finishNativeConstruction(lua_glue::StateView lua,
                              const lua_glue::Table& classTable,
                              const lua_glue::Object& instance);
int superFunction(lua_State* state);

}  // namespace ludork::standard::class_runtime::detail
