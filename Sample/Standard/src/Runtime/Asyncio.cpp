#include "Bindings/Bindings.hpp"

#include "Core/SystemServices.hpp"

#include <sol2/sol.hpp>

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

sol::table tasks(sol::state_view lua) {
    sol::table registry = lua.registry();
    const sol::object value = registry.raw_get<sol::object>(TASKS_KEY);
    if (value.is<sol::table>()) {
        return value.as<sol::table>();
    }
    sol::table result = lua.create_table();
    registry.raw_set(TASKS_KEY, result);
    return result;
}

int createTask(lua_State* state) {
    luaL_checktype(state, 1, LUA_TFUNCTION);
    const int argumentCount = lua_gettop(state) - 1;
    sol::state_view lua(state);
    sol::thread thread = sol::thread::create(state);
    lua_State* threadState = thread.thread_state();
    lua_pushvalue(state, 1);
    lua_xmove(state, threadState, 1);
    for (int index = 2; index <= argumentCount + 1; ++index) {
        lua_pushvalue(state, index);
        lua_xmove(state, threadState, 1);
    }
    sol::table task = lua.create_table_with(
        "thread", thread, "nargs", argumentCount, "started", false, "done",
        false, "cancelled", false, "__asyncioTask", true, "deadline", 0.0);
    tasks(lua).add(task);
    task.push();
    return 1;
}

int cancelTask(lua_State* state) {
    if (lua_type(state, 1) != LUA_TTABLE) {
        return luaL_error(state, "asyncio.cancel_task expects an asyncio task");
    }
    sol::table task = sol::stack::get<sol::table>(state, 1);
    const sol::object marker = task.raw_get<sol::object>("__asyncioTask");
    if (!marker.is<bool>() || !marker.as<bool>()) {
        return luaL_error(state, "asyncio.cancel_task expects an asyncio task");
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

void registerAsyncio(sol::state_view lua) {
    sol::table asyncio = lua.create_table();
    asyncio.set_function("create_task", createTask);
    asyncio.set_function("cancel_task", cancelTask);
    asyncio.set_function("sleep", sleepTask);
    lua["asyncio"] = std::move(asyncio);
}

void updateAsyncio(sol::state_view lua) {
    lua_State* state = lua.lua_state();
    const int baseTop = lua_gettop(state);
    sol::table taskList = tasks(lua);
    const std::size_t originalCount = taskList.size();
    std::size_t writeIndex = 1;
    std::string taskError;
    const double now = performanceCounter();
    for (std::size_t index = 1; index <= originalCount; ++index) {
        const sol::object value = taskList.raw_get<sol::object>(index);
        if (!value.is<sol::table>()) {
            continue;
        }
        sol::table task = value.as<sol::table>();
        const bool done = task.raw_get<bool>("done");
        const bool cancelled = task.raw_get<bool>("cancelled");
        const double deadline = task.raw_get<double>("deadline");
        bool keep = !done && !cancelled;
        if (keep && deadline <= now) {
            const sol::thread thread = task.raw_get<sol::thread>("thread");
            lua_State* threadState = thread.thread_state();
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
                             taskList.raw_get<sol::object>(index));
        }
        for (std::size_t index = writeIndex; index <= currentCount; ++index) {
            taskList.raw_set(index, sol::lua_nil);
        }
    }
    lua_settop(state, baseTop);
    if (!taskError.empty()) {
        throw std::runtime_error(taskError);
    }
}

void shutdownAsyncio(sol::state_view lua) noexcept {
    lua.registry().raw_set(TASKS_KEY, sol::lua_nil);
}

}  // namespace ludork::standard::binding
