#include <LuaError.hpp>
#include "Bindings/Bindings.hpp"

#include "Core/SystemServices.hpp"

#include <LuaGlue/LuaGlue.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace ludork::standard::binding {

namespace {

constexpr const char* TASKS_KEY = "Ludork.Standard.AsyncTasks";

lua_glue::Table tasks(lua_glue::StateView lua) {
    lua_glue::Table registry = lua.registry();
    const lua_glue::Object value =
        registry.raw_get<lua_glue::Object>(TASKS_KEY);
    if (value.is<lua_glue::Table>()) {
        return value.as<lua_glue::Table>();
    }
    lua_glue::Table result = lua.create_table();
    registry.raw_set(TASKS_KEY, result);
    return result;
}

int createTask(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        luaL_checktype(state, 1, LUA_TFUNCTION);
        const int argumentCount = lua_gettop(state) - 1;
        lua_glue::StateView lua(state);
        lua_State* threadState = lua_newthread(state);
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
        tasks(lua).add(task);
        task.push(state);
        return 1;
    });
}

int cancelTask(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
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
        task.raw_set("done", true);
        task.raw_set("deadline", 0.0);
        lua_pushboolean(state, true);
        return 1;
    });
}

int sleepTask(lua_State* state) {
    const double duration = std::max(0.0, luaL_checknumber(state, 1));
    if (lua_isyieldable(state) == 0) {
        return luaL_error(state,
                          "asyncio.sleep must be called from an asyncio task");
    }
    lua_pushnumber(state, duration);
    return lua_yield(state, 1);
}

void clearThread(lua_State* thread) {
    lua_settop(thread, 0);
}

}  // namespace

void registerAsyncio(lua_glue::StateView lua) {
    lua_glue::Table asyncio = lua.create_table();
    asyncio.set_function("create_task", createTask);
    asyncio.set_function("cancel_task", cancelTask);
    asyncio.set_function("sleep", sleepTask);
    lua["asyncio"] = std::move(asyncio);
}

void updateAsyncio(lua_glue::StateView lua) {
    lua_State* state = lua.lua_state();
    const int baseTop = lua_gettop(state);
    lua_glue::Table taskList = tasks(lua);
    const std::size_t originalCount = taskList.size();
    std::size_t writeIndex = 1;
    std::string taskError;
    const double now = performanceCounter();
    for (std::size_t index = 1; index <= originalCount; ++index) {
        const lua_glue::Object value =
            taskList.raw_get<lua_glue::Object>(index);
        if (!value.is<lua_glue::Table>()) {
            continue;
        }
        lua_glue::Table task = value.as<lua_glue::Table>();
        const bool done = task.raw_get<bool>("done");
        const bool cancelled = task.raw_get<bool>("cancelled");
        const double deadline = task.raw_get<double>("deadline");
        bool keep = !done && !cancelled;
        if (keep && deadline <= now) {
            const lua_glue::Object thread =
                task.raw_get<lua_glue::Object>("thread");
            auto pushedThread = lua_glue::PushGuard(thread);
            lua_State* threadState = lua_tothread(state, pushedThread.index());
            if (threadState == nullptr) {
                throw std::runtime_error("asyncio task thread is unavailable");
            }
            const bool started = task.raw_get<bool>("started");
            int argumentCount = 0;
            if (!started) {
                argumentCount = task.raw_get<int>("nargs");
                task.raw_set("started", true);
            }
            int resultCount = 0;
            const int status =
                lua_resume(threadState, state, argumentCount, &resultCount);
            if (status == LUA_YIELD) {
                double delay = 0.0;
                if (resultCount > 0 &&
                    lua_isnumber(threadState, -resultCount)) {
                    delay =
                        std::max(0.0, lua_tonumber(threadState, -resultCount));
                }
                clearThread(threadState);
                task.raw_set("deadline", performanceCounter() + delay);
            } else if (status == LUA_OK) {
                clearThread(threadState);
                task.raw_set("done", true);
                keep = false;
            } else {
                const char* message = lua_tostring(threadState, -1);
                taskError =
                    message == nullptr ? "asyncio task failed" : message;
                clearThread(threadState);
                task.raw_set("error", taskError);
                task.raw_set("done", true);
                keep = false;
            }
        }
        if (keep) {
            taskList.raw_set(writeIndex++, task);
        }
        if (!taskError.empty()) {
            break;
        }
    }
    if (taskError.empty()) {
        const std::size_t currentCount = taskList.size();
        for (std::size_t index = originalCount + 1; index <= currentCount;
             ++index) {
            taskList.raw_set(writeIndex++,
                             taskList.raw_get<lua_glue::Object>(index));
        }
        for (std::size_t index = writeIndex; index <= currentCount; ++index) {
            taskList.raw_set(index, lua_glue::nil);
        }
    }
    lua_settop(state, baseTop);
    if (!taskError.empty()) {
        throw std::runtime_error(taskError);
    }
}

void shutdownAsyncio(lua_glue::StateView lua) noexcept {
    lua.registry().raw_set(TASKS_KEY, lua_glue::nil);
}

}  // namespace ludork::standard::binding
