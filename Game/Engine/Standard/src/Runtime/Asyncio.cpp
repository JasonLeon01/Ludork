#include <LuaError.hpp>
#include "Bindings/Bindings.hpp"

#include "Core/SystemServices.hpp"

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace ludork::standard::binding {

namespace {

constexpr const char* TASKS_KEY = "Ludork.Standard.AsyncTasks";
constexpr const char* THREAD_POOL_KEY = "Ludork.Standard.AsyncThreadPool";
constexpr const char* ACTIVE_TASK_KEY = "Ludork.Standard.ActiveAsyncTask";
constexpr const char* STOPPING_KEY = "Ludork.Standard.AsyncTasksStopping";

lua_glue::Table registryTable(lua_glue::StateView lua, const char* key) {
    lua_glue::Table registry = lua.registry();
    const lua_glue::Object value = registry.raw_get<lua_glue::Object>(key);
    if (value.is<lua_glue::Table>()) {
        return value.as<lua_glue::Table>();
    }
    lua_glue::Table result = lua.create_table();
    registry.raw_set(key, result);
    return result;
}

int activeTask(lua_State* state) {
    lua_getfield(state, LUA_REGISTRYINDEX, ACTIVE_TASK_KEY);
    if (lua_istable(state, -1)) {
        lua_getfield(state, -1, "thread");
        const bool matches = lua_tothread(state, -1) == state;
        lua_pop(state, 1);
        if (matches) {
            return lua_gettop(state);
        }
    }
    lua_pop(state, 1);
    return 0;
}

lua_State* taskThread(lua_State* state, const lua_glue::Table& task) {
    task.raw_get<lua_glue::Object>("thread").push(state);
    lua_State* thread = lua_tothread(state, -1);
    lua_pop(state, 1);
    return thread;
}

std::string closeTask(lua_State* state, lua_glue::Table& task) {
    task.raw_set("done", true);
    task.raw_set("waiting", lua_glue::nil);
    task.raw_set("deadline", 0.0);
    std::string error;
    lua_State* thread = taskThread(state, task);
    if (thread != nullptr) {
        if (lua_closethread(thread, state) != LUA_OK) {
            const char* message = lua_tostring(thread, -1);
            error = message == nullptr ? "asyncio task close failed" : message;
        }
        lua_settop(thread, 0);
        lua_glue::StateView lua(state);
        if (!lua.registry()
                 .raw_get<lua_glue::Object>(STOPPING_KEY)
                 .is<bool>()) {
            registryTable(lua, THREAD_POOL_KEY)
                .add(task.raw_get<lua_glue::Object>("thread"));
        }
    }
    task.raw_set("thread", lua_glue::nil);
    if (!error.empty()) {
        task.raw_set("error", error);
    }
    return error;
}

int createTask(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        luaL_checktype(state, 1, LUA_TFUNCTION);
        const int argumentCount = lua_gettop(state) - 1;
        lua_glue::StateView lua(state);
        if (lua.registry().raw_get<lua_glue::Object>(STOPPING_KEY).is<bool>()) {
            throw std::runtime_error("asyncio is shutting down");
        }
        // LuaGlue retains coroutine aliases until session shutdown; reuse
        // closed shells to bound retention by concurrent task demand.
        lua_glue::Table pool = registryTable(lua, THREAD_POOL_KEY);
        const std::size_t available = pool.size();
        lua_State* threadState = nullptr;
        if (available > 0) {
            pool.raw_get<lua_glue::Object>(available).push(state);
            pool.raw_set(available, lua_glue::nil);
            threadState = lua_tothread(state, -1);
            if (lua_closethread(threadState, state) != LUA_OK) {
                throw std::runtime_error("Unable to reset asyncio coroutine");
            }
        } else {
            threadState = lua_newthread(state);
            if (lua_glue::InitializeState(threadState) != 0) {
                throw std::runtime_error(
                    "Unable to initialize asyncio coroutine");
            }
        }
        const lua_glue::Object thread =
            lua_glue::Read<lua_glue::Object>(state, -1);
        lua_pop(state, 1);
        lua_pushvalue(state, 1);
        lua_xmove(state, threadState, 1);
        for (int index = 2; index <= argumentCount + 1; ++index) {
            lua_pushvalue(state, index);
            lua_xmove(state, threadState, 1);
        }
        lua_glue::Table task = lua.create_table_with(
            "thread", thread, "nargs", argumentCount, "started", false, "done",
            false, "cancelled", false, "__asyncioTask", true, "deadline", 0.0);
        registryTable(lua, TASKS_KEY).add(task);
        task.push(state);
        return 1;
    });
}

int cancelTask(lua_State* state) {
    const int result =
        ludork::standard::protectedLuaCallback(state, [&]() -> int {
            if (lua_type(state, 1) != LUA_TTABLE) {
                throw std::invalid_argument(
                    "asyncio.cancel_task expects an asyncio task");
            }
            lua_glue::Table task = lua_glue::Read<lua_glue::Table>(state, 1);
            const lua_glue::Object marker =
                task.raw_get<lua_glue::Object>("__asyncioTask");
            if (!marker.is<bool>() || !marker.as<bool>()) {
                throw std::invalid_argument(
                    "asyncio.cancel_task expects an asyncio task");
            }
            if (task.raw_get<bool>("done") || task.raw_get<bool>("cancelled")) {
                lua_pushboolean(state, false);
                return 1;
            }
            task.raw_set("cancelled", true);
            if (taskThread(state, task) != state) {
                const std::string error = closeTask(state, task);
                if (!error.empty()) {
                    throw std::runtime_error(error);
                }
            }
            lua_pushboolean(state, true);
            return 1;
        });
    const int active = activeTask(state);
    if (active != 0) {
        lua_getfield(state, active, "cancelled");
        const bool cancelled = lua_toboolean(state, -1) != 0;
        lua_pop(state, 2);
        if (cancelled) {
            return lua_yield(state, 0);
        }
    }
    return result;
}

int sleepTask(lua_State* state) {
    const double duration = std::max(0.0, luaL_checknumber(state, 1));
    if (lua_isyieldable(state) == 0 || activeTask(state) == 0) {
        return luaL_error(state,
                          "asyncio.sleep must be called from an asyncio task");
    }
    lua_pushnumber(state, duration);
    return lua_yield(state, 1);
}

int awaitResult(lua_State*, int, lua_KContext) {
    return 1;
}

int awaitOperation(lua_State* state) {
    if (lua_isyieldable(state) == 0) {
        return luaL_error(state,
                          "asyncio.await must be called from an asyncio task");
    }
    lua_settop(state, 1);
    const int task = activeTask(state);
    if (task == 0) {
        return luaL_error(state,
                          "asyncio.await must be called from an asyncio task");
    }
    lua_getfield(state, 1, "getStatus");
    lua_pushvalue(state, 1);
    lua_call(state, 1, 1);
    const char* status = lua_tostring(state, -1);
    if (status == nullptr) {
        return luaL_error(state,
                          "asyncio.await operation status must be a string");
    }
    if (std::strcmp(status, "completed") == 0) {
        lua_getfield(state, 1, "getResult");
        lua_pushvalue(state, 1);
        lua_call(state, 1, 1);
        return 1;
    }
    if (std::strcmp(status, "cancelled") == 0) {
        lua_pushboolean(state, true);
        lua_setfield(state, task, "cancelled");
        return lua_yield(state, 0);
    }
    if (std::strcmp(status, "pending") != 0) {
        return luaL_error(state,
                          "asyncio.await received an invalid operation status");
    }
    lua_pushvalue(state, 1);
    lua_setfield(state, task, "waiting");
    return lua_yieldk(state, 0, 0, awaitResult);
}

int operationMethod(lua_State* state) {
    lua_getfield(state, 1, lua_tostring(state, 2));
    lua_pushvalue(state, 1);
    lua_call(state, 1, 1);
    return 1;
}

lua_glue::Object callOperation(lua_State* state,
                               const lua_glue::Object& operation,
                               const char* method) {
    lua_pushcfunction(state, operationMethod);
    operation.push(state);
    lua_pushstring(state, method);
    if (ludork::standard::protectedLuaCall(state, 2, 1) != LUA_OK) {
        const std::string error = ludork::standard::luaErrorMessage(state, -1);
        lua_pop(state, 1);
        throw std::runtime_error(error);
    }
    lua_glue::Object result = lua_glue::Read<lua_glue::Object>(state, -1);
    lua_pop(state, 1);
    return result;
}

void updateTask(lua_glue::StateView lua, lua_glue::Table& task, double now) {
    lua_State* state = lua.lua_state();
    if (task.raw_get<bool>("done")) {
        return;
    }
    if (task.raw_get<bool>("cancelled")) {
        const std::string error = closeTask(state, task);
        if (!error.empty()) {
            throw std::runtime_error(error);
        }
        return;
    }
    if (task.raw_get<double>("deadline") > now) {
        return;
    }
    lua_State* thread = taskThread(state, task);
    if (thread == nullptr) {
        throw std::runtime_error("asyncio task thread is unavailable");
    }
    int argumentCount = 0;
    const lua_glue::Object waiting = task.raw_get<lua_glue::Object>("waiting");
    if (waiting.valid() && !waiting.is<lua_glue::Nil>()) {
        const lua_glue::Object status =
            callOperation(state, waiting, "getStatus");
        if (!status.is<std::string>()) {
            throw std::runtime_error(
                "asyncio.await operation status must be a string");
        }
        const std::string name = status.as<std::string>();
        if (name == "pending") {
            return;
        }
        if (name == "cancelled") {
            task.raw_set("cancelled", true);
            const std::string error = closeTask(state, task);
            if (!error.empty()) {
                throw std::runtime_error(error);
            }
            return;
        }
        if (name != "completed") {
            throw std::runtime_error(
                "asyncio.await received an invalid operation status");
        }
        callOperation(state, waiting, "getResult").push(thread);
        task.raw_set("waiting", lua_glue::nil);
        argumentCount = 1;
    } else if (!task.raw_get<bool>("started")) {
        argumentCount = task.raw_get<int>("nargs");
        task.raw_set("started", true);
    }
    lua.registry().raw_set(ACTIVE_TASK_KEY, task);
    int resultCount = 0;
    const int status = lua_resume(thread, state, argumentCount, &resultCount);
    lua.registry().raw_set(ACTIVE_TASK_KEY, lua_glue::nil);
    if (status != LUA_OK && status != LUA_YIELD) {
        const char* message = lua_tostring(thread, -1);
        throw std::runtime_error(message == nullptr ? "asyncio task failed"
                                                    : message);
    }
    if (status == LUA_OK || task.raw_get<bool>("cancelled")) {
        const std::string error = closeTask(state, task);
        if (!error.empty()) {
            throw std::runtime_error(error);
        }
        return;
    }
    double delay = 0.0;
    if (resultCount > 0 && lua_isnumber(thread, -resultCount)) {
        delay = std::max(0.0, lua_tonumber(thread, -resultCount));
    }
    lua_settop(thread, 0);
    task.raw_set("deadline", performanceCounter() + delay);
}

}  // namespace

void registerAsyncio(lua_glue::StateView lua) {
    lua.registry().raw_set(STOPPING_KEY, lua_glue::nil);
    lua_glue::Table asyncio = lua.create_table();
    asyncio.set_function("create_task", createTask);
    asyncio.set_function("cancel_task", cancelTask);
    asyncio.set_function("sleep", sleepTask);
    asyncio.set_function("await", awaitOperation);
    lua["asyncio"] = std::move(asyncio);
}

void updateAsyncio(lua_glue::StateView lua) {
    lua_State* state = lua.lua_state();
    const int baseTop = lua_gettop(state);
    lua_glue::Table taskList = registryTable(lua, TASKS_KEY);
    const std::size_t originalCount = taskList.size();
    std::string taskError;
    const double now = performanceCounter();
    for (std::size_t index = 1; index <= originalCount; ++index) {
        lua_glue::Table task = taskList.raw_get<lua_glue::Table>(index);
        try {
            updateTask(lua, task, now);
        } catch (const std::exception& error) {
            taskError = error.what();
            const std::string closeError = closeTask(state, task);
            if (!closeError.empty() && closeError != taskError) {
                taskError += "\n" + closeError;
            }
            task.raw_set("error", taskError);
            break;
        }
    }
    std::size_t writeIndex = 1;
    const std::size_t currentCount = taskList.size();
    for (std::size_t index = 1; index <= currentCount; ++index) {
        lua_glue::Table task = taskList.raw_get<lua_glue::Table>(index);
        if (!task.raw_get<bool>("done")) {
            taskList.raw_set(writeIndex++, task);
        }
    }
    for (std::size_t index = writeIndex; index <= currentCount; ++index) {
        taskList.raw_set(index, lua_glue::nil);
    }
    lua_settop(state, baseTop);
    if (!taskError.empty()) {
        throw std::runtime_error(taskError);
    }
}

void shutdownAsyncio(lua_glue::StateView lua) noexcept {
    lua.registry().raw_set(STOPPING_KEY, true);
    lua_glue::Table taskList = registryTable(lua, TASKS_KEY);
    const std::size_t count = taskList.size();
    for (std::size_t index = 1; index <= count; ++index) {
        lua_glue::Table task = taskList.raw_get<lua_glue::Table>(index);
        task.raw_set("cancelled", true);
        const std::string error = closeTask(lua.lua_state(), task);
        if (!error.empty()) {
            std::fprintf(stderr, "Asyncio task shutdown failed: %s\n",
                         error.c_str());
        }
    }
    lua.registry().raw_set(ACTIVE_TASK_KEY, lua_glue::nil);
    lua.registry().raw_set(TASKS_KEY, lua_glue::nil);
    lua.registry().raw_set(THREAD_POOL_KEY, lua_glue::nil);
}

}  // namespace ludork::standard::binding
