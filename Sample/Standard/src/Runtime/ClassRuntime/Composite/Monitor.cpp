#include "Composite/CompositeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeBridge.hpp"
#include "Detail/RuntimeState.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lua.h>
}

#include <climits>
#include <stdexcept>
#include <string>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Monitor
// ───────────────────────────────────────────────────────────────────

sol::object monitorMissing(sol::state_view lua) {
    const sol::object rawClass = lua.globals().raw_get<sol::object>("Class");
    if (rawClass.is<sol::table>()) {
        sol::table classModule = rawClass.as<sol::table>();
        const sol::object missing = classModule.raw_get<sol::object>("MISSING");
        if (missing.is<sol::table>()) {
            return missing;
        }
        sol::table created = lua.create_table();
        classModule.raw_set("MISSING", created);
        return created;
    }
    return lua.create_table();
}

sol::table monitorState(sol::state_view lua, const sol::object& target) {
    const sol::object rawState = registryTable(lua, MONITOR_STATES_KEY, "k")
                                     .raw_get<sol::object>(target);
    return rawState.is<sol::table>() ? rawState.as<sol::table>()
                                     : lua.create_table();
}

sol::object originalMonitoredIndex(sol::state_view lua, const sol::table& state,
                                   const sol::object& target,
                                   const sol::object& key) {
    const sol::object originalIndex = state.raw_get<sol::object>("index");
    if (originalIndex.is<sol::protected_function>()) {
        sol::protected_function_result result =
            originalIndex.as<sol::protected_function>()(target, key);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return result.get<sol::object>();
    }
    if (originalIndex.is<sol::table>()) {
        return originalIndex.as<sol::table>().raw_get<sol::object>(key);
    }
    return target.as<sol::table>().raw_get<sol::object>(key);
}

void originalMonitoredNewIndex(sol::state_view lua, const sol::table& state,
                               const sol::object& target,
                               const sol::object& key,
                               const sol::object& value) {
    const sol::object originalNewIndex = state.raw_get<sol::object>("newIndex");
    if (originalNewIndex.is<sol::protected_function>()) {
        sol::protected_function_result result =
            originalNewIndex.as<sol::protected_function>()(target, key, value);
        if (!result.valid()) {
            const sol::error error = result;
            throw std::runtime_error(error.what());
        }
        return;
    }
    if (originalNewIndex.is<sol::table>()) {
        originalNewIndex.as<sol::table>().raw_set(key, value);
        return;
    }
    target.as<sol::table>().raw_set(key, value);
}

void invokeMonitorCallback(sol::state_view lua, sol::table entry,
                           const sol::object& oldValue,
                           const sol::object& newValue) {
    if (luaValuesEqual(lua, oldValue, newValue)) {
        return;
    }
    const sol::object rawRunning = entry.raw_get<sol::object>("running");
    if (rawRunning.is<bool>() && rawRunning.as<bool>()) {
        return;
    }
    const sol::object rawCallback = entry.raw_get<sol::object>("callback");
    if (!rawCallback.is<sol::protected_function>()) {
        return;
    }
    std::vector<sol::object> arguments{oldValue, newValue};
    const sol::object rawParams = entry.raw_get<sol::object>("params");
    if (rawParams.is<sol::table>()) {
        const sol::table params = rawParams.as<sol::table>();
        if (params.size() >
            static_cast<std::size_t>(INT_MAX) - arguments.size()) {
            throw std::length_error("Monitor callback argument count overflow");
        }
        arguments.reserve(arguments.size() + params.size());
        for (std::size_t index = 1; index <= params.size(); ++index) {
            arguments.push_back(params.raw_get<sol::object>(index));
        }
    }
    entry.raw_set("running", true);
    lua_State* state = lua.lua_state();
    const int stackBase = lua_gettop(state);
    try {
        static_cast<void>(invokeRuntimeFunction(lua, rawCallback, arguments,
                                                "monitor callback arguments"));
        lua_settop(state, stackBase);
        entry.raw_set("running", false);
    } catch (...) {
        lua_settop(state, stackBase);
        entry.raw_set("running", false);
        throw;
    }
}

sol::object monitoredTableIndex(sol::object target, sol::object key,
                                sol::this_state state) {
    sol::state_view lua(state);
    const sol::table monitor = monitorState(lua, target);
    const sol::object rawFields = monitor.raw_get<sol::object>("fields");
    if (rawFields.is<sol::table>()) {
        const sol::object rawEntry =
            rawFields.as<sol::table>().raw_get<sol::object>(key);
        if (rawEntry.is<sol::table>()) {
            const sol::table entry = rawEntry.as<sol::table>();
            const sol::object rawHasValue =
                entry.raw_get<sol::object>("hasValue");
            if (rawHasValue.is<bool>() && rawHasValue.as<bool>()) {
                return entry.raw_get<sol::object>("value");
            }
            return nilObject(lua);
        }
    }
    return originalMonitoredIndex(lua, monitor, target, key);
}

void monitoredTableNewIndex(sol::object target, sol::object key,
                            sol::object value, sol::this_state state) {
    sol::state_view lua(state);
    const sol::table monitor = monitorState(lua, target);
    const sol::object rawFields = monitor.raw_get<sol::object>("fields");
    const sol::object rawEntry =
        rawFields.is<sol::table>()
            ? rawFields.as<sol::table>().raw_get<sol::object>(key)
            : nilObject(lua);
    if (!rawEntry.is<sol::table>()) {
        originalMonitoredNewIndex(lua, monitor, target, key, value);
        return;
    }
    if (!value.valid() || value.get_type() == sol::type::lua_nil) {
        throw std::invalid_argument("Monitored fields cannot be assigned nil");
    }
    sol::table entry = rawEntry.as<sol::table>();
    const sol::object rawHasValue = entry.raw_get<sol::object>("hasValue");
    const sol::object oldValue =
        rawHasValue.is<bool>() && rawHasValue.as<bool>()
            ? entry.raw_get<sol::object>("value")
            : entry.raw_get<sol::object>("missing");
    entry.raw_set("value", value);
    entry.raw_set("hasValue", true);
    entry.raw_set("assigned", true);
    invokeMonitorCallback(lua, entry, oldValue, value);
}

sol::table createTableMonitorState(sol::state_view lua, sol::table target) {
    lua_State* state = lua.lua_state();
    target.push();
    sol::object originalMetatable = nilObject(lua);
    if (lua_getmetatable(state, -1) != 0) {
        originalMetatable = sol::stack::get<sol::object>(state, -1);
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    if (originalMetatable.is<sol::table>()) {
        const sol::object protection =
            originalMetatable.as<sol::table>().raw_get<sol::object>(
                "__metatable");
        if (protection.valid() && protection.get_type() != sol::type::lua_nil) {
            throw std::invalid_argument(
                "Lua monitors cannot replace a protected metatable");
        }
    }
    sol::table monitor = lua.create_table();
    sol::table fields = lua.create_table();
    monitor.raw_set("meta", originalMetatable);
    monitor.raw_set("fields", fields);
    if (originalMetatable.is<sol::table>()) {
        sol::table original = originalMetatable.as<sol::table>();
        monitor.raw_set("index", original.raw_get<sol::object>("__index"));
        monitor.raw_set("newIndex",
                        original.raw_get<sol::object>("__newindex"));
    }
    sol::table proxy = lua.create_table();
    proxy.set_function("__index", &monitoredTableIndex);
    proxy.set_function("__newindex", &monitoredTableNewIndex);
    target.push();
    proxy.push();
    lua_setmetatable(state, -2);
    lua_pop(state, 1);
    registryTable(lua, MONITOR_STATES_KEY, "k").raw_set(target, monitor);
    return monitor;
}

void registerMonitor(sol::this_state state, const sol::object& target,
                     const std::string& name,
                     const sol::protected_function& callback,
                     sol::optional<sol::table> params) {
    sol::state_view lua(state);
    if (name.empty()) {
        throw std::invalid_argument("Monitor field name must not be empty");
    }
    if (target.get_type() == sol::type::table) {
        sol::table object = target.as<sol::table>();
        sol::table monitor = monitorState(lua, target);
        if (!monitor.raw_get<sol::object>("fields").is<sol::table>()) {
            monitor = createTableMonitorState(lua, object);
        }
        sol::table fields = monitor.raw_get<sol::table>("fields");
        const sol::object rawEntry = fields.raw_get<sol::object>(name);
        if (rawEntry.is<sol::table>()) {
            sol::table entry = rawEntry.as<sol::table>();
            entry.raw_set("callback", callback);
            entry.raw_set("params", params.value_or(lua.create_table()));
            return;
        }
        const sol::object rawValue = object.raw_get<sol::object>(name);
        sol::object value = rawValue;
        if (!value.valid() || value.get_type() == sol::type::lua_nil) {
            value = originalMonitoredIndex(lua, monitor, target,
                                           sol::make_object(lua, name));
        }
        sol::table entry = lua.create_table();
        const bool hasValue =
            value.valid() && value.get_type() != sol::type::lua_nil;
        entry.raw_set("hasValue", hasValue);
        if (hasValue) {
            entry.raw_set("value", value);
        }
        entry.raw_set("callback", callback);
        entry.raw_set("params", params.value_or(lua.create_table()));
        entry.raw_set("running", false);
        entry.raw_set("raw", rawValue.valid() &&
                                 rawValue.get_type() != sol::type::lua_nil);
        entry.raw_set("assigned", false);
        entry.raw_set("missing", monitorMissing(lua));
        fields.raw_set(name, entry);
        object.raw_set(name, sol::lua_nil);
        return;
    }
    if (target.get_type() != sol::type::userdata) {
        throw std::invalid_argument(
            "Monitors require a table or userdata target");
    }
    sol::table fields = class_native::getUserFields(lua, target, true);
    const sol::object rawCallbacks =
        fields.raw_get<sol::object>("__monitorCallbacks");
    sol::table callbacks = rawCallbacks.is<sol::table>()
                               ? rawCallbacks.as<sol::table>()
                               : lua.create_table();
    if (!rawCallbacks.is<sol::table>()) {
        fields.raw_set("__monitorCallbacks", callbacks);
    }
    sol::table entry = lua.create_table();
    entry.raw_set("callback", callback);
    entry.raw_set("params", params.value_or(lua.create_table()));
    entry.raw_set("running", false);
    entry.raw_set("missing", monitorMissing(lua));
    callbacks.raw_set(name, entry);
}

void unregisterMonitor(sol::this_state state, const sol::object& target,
                       const std::string& name) {
    sol::state_view lua(state);
    if (target.get_type() == sol::type::table) {
        sol::table object = target.as<sol::table>();
        sol::table monitor = monitorState(lua, target);
        const sol::object rawFields = monitor.raw_get<sol::object>("fields");
        if (!rawFields.is<sol::table>()) {
            return;
        }
        sol::table fields = rawFields.as<sol::table>();
        const sol::object rawEntry = fields.raw_get<sol::object>(name);
        if (!rawEntry.is<sol::table>()) {
            return;
        }
        sol::table entry = rawEntry.as<sol::table>();
        fields.raw_set(name, sol::lua_nil);
        const bool restore =
            rawBool(entry, "raw") || rawBool(entry, "assigned");
        if (restore) {
            const bool hasValue = rawBool(entry, "hasValue");
            object.raw_set(name, hasValue ? entry.raw_get<sol::object>("value")
                                          : nilObject(lua));
        }
        if (!tableIsEmpty(fields)) {
            return;
        }
        const sol::object originalMetatable =
            monitor.raw_get<sol::object>("meta");
        object.push();
        if (originalMetatable.valid() &&
            originalMetatable.get_type() != sol::type::lua_nil) {
            originalMetatable.push();
        } else {
            lua_pushnil(lua.lua_state());
        }
        lua_setmetatable(lua.lua_state(), -2);
        lua_pop(lua.lua_state(), 1);
        registryTable(lua, MONITOR_STATES_KEY, "k")
            .raw_set(object, sol::lua_nil);
        return;
    }
    if (target.get_type() != sol::type::userdata) {
        return;
    }
    sol::table fields = class_native::getUserFields(lua, target, true);
    const sol::object rawCallbacks =
        fields.raw_get<sol::object>("__monitorCallbacks");
    if (!rawCallbacks.is<sol::table>()) {
        return;
    }
    sol::table callbacks = rawCallbacks.as<sol::table>();
    callbacks.raw_set(name, sol::lua_nil);
    if (tableIsEmpty(callbacks)) {
        fields.raw_set("__monitorCallbacks", sol::lua_nil);
    }
}

}  // namespace ludork::standard::class_runtime::detail
