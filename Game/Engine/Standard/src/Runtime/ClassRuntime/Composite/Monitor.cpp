#include "Composite/CompositeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeBridge.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypedFields.hpp"

#include <LuaGlue/LuaGlue.hpp>

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

lua_glue::Object monitorMissing(lua_glue::StateView lua) {
    const lua_glue::Object rawClass =
        lua.globals().raw_get<lua_glue::Object>("Class");
    if (rawClass.is<lua_glue::Table>()) {
        lua_glue::Table classModule = rawClass.as<lua_glue::Table>();
        const lua_glue::Object missing =
            classModule.raw_get<lua_glue::Object>("MISSING");
        if (missing.is<lua_glue::Table>()) {
            return missing;
        }
        lua_glue::Table created = lua.create_table();
        classModule.raw_set("MISSING", created);
        return created;
    }
    return lua.create_table();
}

lua_glue::Table monitorState(lua_glue::StateView lua,
                             const lua_glue::Object& target) {
    const lua_glue::Object rawState =
        registryTable(lua, MONITOR_STATES_KEY, "k")
            .raw_get<lua_glue::Object>(target);
    return rawState.is<lua_glue::Table>() ? rawState.as<lua_glue::Table>()
                                          : lua.create_table();
}

lua_glue::Object originalMonitoredIndex(lua_glue::StateView lua,
                                        const lua_glue::Table& state,
                                        const lua_glue::Object& target,
                                        const lua_glue::Object& key) {
    const lua_glue::Object originalIndex =
        state.raw_get<lua_glue::Object>("index");
    if (originalIndex.is<lua_glue::Function>()) {
        lua_glue::CallResult result =
            originalIndex.as<lua_glue::Function>()(target, key);
        if (!result.valid()) {
            const std::string error = result.error();
            throw std::runtime_error(error.c_str());
        }
        return result.get<lua_glue::Object>();
    }
    if (originalIndex.is<lua_glue::Table>()) {
        return originalIndex.as<lua_glue::Table>().raw_get<lua_glue::Object>(
            key);
    }
    return target.as<lua_glue::Table>().raw_get<lua_glue::Object>(key);
}

void originalMonitoredNewIndex(lua_glue::StateView lua,
                               const lua_glue::Table& state,
                               const lua_glue::Object& target,
                               const lua_glue::Object& key,
                               const lua_glue::Object& value) {
    const lua_glue::Object originalNewIndex =
        state.raw_get<lua_glue::Object>("newIndex");
    if (originalNewIndex.is<lua_glue::Function>()) {
        lua_glue::CallResult result =
            originalNewIndex.as<lua_glue::Function>()(target, key, value);
        if (!result.valid()) {
            const std::string error = result.error();
            throw std::runtime_error(error.c_str());
        }
        return;
    }
    if (originalNewIndex.is<lua_glue::Table>()) {
        originalNewIndex.as<lua_glue::Table>().raw_set(key, value);
        return;
    }
    target.as<lua_glue::Table>().raw_set(key, value);
}

void invokeMonitorCallback(lua_glue::StateView lua, lua_glue::Table entry,
                           const lua_glue::Object& oldValue,
                           const lua_glue::Object& newValue) {
    if (!rawBool(entry, "notifyEqualWrites") &&
        luaValuesEqual(lua, oldValue, newValue)) {
        return;
    }
    const lua_glue::Object rawRunning =
        entry.raw_get<lua_glue::Object>("running");
    if (rawRunning.is<bool>() && rawRunning.as<bool>()) {
        return;
    }
    const lua_glue::Object rawCallback =
        entry.raw_get<lua_glue::Object>("callback");
    if (!rawCallback.is<lua_glue::Function>()) {
        return;
    }
    std::vector<lua_glue::Object> arguments{oldValue, newValue};
    const lua_glue::Object rawParams =
        entry.raw_get<lua_glue::Object>("params");
    if (rawParams.is<lua_glue::Table>()) {
        const lua_glue::Table params = rawParams.as<lua_glue::Table>();
        if (params.size() >
            static_cast<std::size_t>(INT_MAX) - arguments.size()) {
            throw std::length_error("Monitor callback argument count overflow");
        }
        arguments.reserve(arguments.size() + params.size());
        for (std::size_t index = 1; index <= params.size(); ++index) {
            arguments.push_back(params.raw_get<lua_glue::Object>(index));
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

void invokeMonitorCallbacks(lua_glue::StateView lua,
                            const lua_glue::Table& entry,
                            const lua_glue::Object& oldValue,
                            const lua_glue::Object& newValue) {
    const lua_glue::Table callbacks =
        entry.raw_get<lua_glue::Table>("callbacks");
    std::vector<lua_glue::Table> snapshot;
    snapshot.reserve(callbacks.size());
    for (std::size_t index = 1; index <= callbacks.size(); ++index) {
        snapshot.push_back(callbacks.raw_get<lua_glue::Table>(index));
    }
    for (const lua_glue::Table& callback : snapshot) {
        if (rawBool(callback, "active")) {
            invokeMonitorCallback(lua, callback, oldValue, newValue);
        }
    }
}

void registerMonitorCallback(lua_glue::StateView lua, lua_glue::Table entry,
                             const lua_glue::Function& callback,
                             const lua_glue::Table& params,
                             bool notifyEqualWrites,
                             const std::string& identifier) {
    const lua_glue::Object rawCallbacks =
        entry.raw_get<lua_glue::Object>("callbacks");
    lua_glue::Table callbacks = rawCallbacks.is<lua_glue::Table>()
                                    ? rawCallbacks.as<lua_glue::Table>()
                                    : lua.create_table();
    entry.raw_set("callbacks", callbacks);
    std::size_t position = callbacks.size() + 1;
    for (std::size_t index = 1; index <= callbacks.size(); ++index) {
        lua_glue::Table existing = callbacks.raw_get<lua_glue::Table>(index);
        if (existing.raw_get<std::string>("identifier") == identifier) {
            existing.raw_set("active", false);
            position = index;
            break;
        }
    }
    lua_glue::Table subscription = lua.create_table();
    subscription.raw_set("identifier", identifier);
    subscription.raw_set("callback", callback);
    subscription.raw_set("params", params);
    subscription.raw_set("notifyEqualWrites", notifyEqualWrites);
    subscription.raw_set("running", false);
    subscription.raw_set("active", true);
    callbacks.raw_set(position, subscription);
}

bool unregisterMonitorCallback(const lua_glue::Table& entry,
                               const std::string& identifier) {
    lua_glue::Table callbacks = entry.raw_get<lua_glue::Table>("callbacks");
    const std::size_t count = callbacks.size();
    for (std::size_t index = 1; index <= count; ++index) {
        lua_glue::Table existing = callbacks.raw_get<lua_glue::Table>(index);
        if (existing.raw_get<std::string>("identifier") != identifier) {
            continue;
        }
        existing.raw_set("active", false);
        for (std::size_t next = index + 1; next <= count; ++next) {
            callbacks.raw_set(next - 1,
                              callbacks.raw_get<lua_glue::Table>(next));
        }
        callbacks.raw_set(count, lua_glue::nil);
        return count == 1;
    }
    return false;
}

lua_glue::Object monitoredTableIndex(lua_glue::Object target,
                                     lua_glue::Object key,
                                     lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    const lua_glue::Table monitor = monitorState(lua, target);
    const lua_glue::Object rawFields =
        monitor.raw_get<lua_glue::Object>("fields");
    if (rawFields.is<lua_glue::Table>()) {
        const lua_glue::Object rawEntry =
            rawFields.as<lua_glue::Table>().raw_get<lua_glue::Object>(key);
        if (rawEntry.is<lua_glue::Table>()) {
            const lua_glue::Table entry = rawEntry.as<lua_glue::Table>();
            const lua_glue::Object rawHasValue =
                entry.raw_get<lua_glue::Object>("hasValue");
            if (rawHasValue.is<bool>() && rawHasValue.as<bool>()) {
                return entry.raw_get<lua_glue::Object>("value");
            }
            return nilObject(lua);
        }
    }
    return originalMonitoredIndex(lua, monitor, target, key);
}

void monitoredTableNewIndex(lua_glue::Object target, lua_glue::Object key,
                            lua_glue::Object value, lua_glue::ThisState state) {
    lua_glue::StateView lua(state);
    const lua_glue::Table monitor = monitorState(lua, target);
    const lua_glue::Object rawFields =
        monitor.raw_get<lua_glue::Object>("fields");
    const lua_glue::Object rawEntry =
        rawFields.is<lua_glue::Table>()
            ? rawFields.as<lua_glue::Table>().raw_get<lua_glue::Object>(key)
            : nilObject(lua);
    if (!rawEntry.is<lua_glue::Table>()) {
        originalMonitoredNewIndex(lua, monitor, target, key, value);
        clearExplicitNilField(lua, target, key);
        return;
    }
    if (!value.valid() || value.get_type() == lua_glue::Type::Nil) {
        throw std::invalid_argument("Monitored fields cannot be assigned nil");
    }
    lua_glue::Table entry = rawEntry.as<lua_glue::Table>();
    const lua_glue::Object rawHasValue =
        entry.raw_get<lua_glue::Object>("hasValue");
    const lua_glue::Object oldValue =
        rawHasValue.is<bool>() && rawHasValue.as<bool>()
            ? entry.raw_get<lua_glue::Object>("value")
            : entry.raw_get<lua_glue::Object>("missing");
    entry.raw_set("value", value);
    entry.raw_set("hasValue", true);
    entry.raw_set("assigned", true);
    clearExplicitNilField(lua, target, key);
    invokeMonitorCallbacks(lua, entry, oldValue, value);
}

lua_glue::Table createTableMonitorState(lua_glue::StateView lua,
                                        lua_glue::Table target) {
    lua_State* state = lua.lua_state();
    target.push(lua.lua_state());
    lua_glue::Object originalMetatable = nilObject(lua);
    if (lua_getmetatable(state, -1) != 0) {
        originalMetatable = lua_glue::Read<lua_glue::Object>(state, -1);
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    if (originalMetatable.is<lua_glue::Table>()) {
        const lua_glue::Object protection =
            originalMetatable.as<lua_glue::Table>().raw_get<lua_glue::Object>(
                "__metatable");
        if (protection.valid() &&
            protection.get_type() != lua_glue::Type::Nil) {
            throw std::invalid_argument(
                "Lua monitors cannot replace a protected metatable");
        }
    }
    lua_glue::Table monitor = lua.create_table();
    lua_glue::Table fields = lua.create_table();
    monitor.raw_set("meta", originalMetatable);
    monitor.raw_set("fields", fields);
    if (originalMetatable.is<lua_glue::Table>()) {
        lua_glue::Table original = originalMetatable.as<lua_glue::Table>();
        monitor.raw_set("index", original.raw_get<lua_glue::Object>("__index"));
        monitor.raw_set("newIndex",
                        original.raw_get<lua_glue::Object>("__newindex"));
    }
    lua_glue::Table proxy = lua.create_table();
    proxy.set_function("__index", &monitoredTableIndex);
    proxy.set_function("__newindex", &monitoredTableNewIndex);
    target.push(lua.lua_state());
    proxy.push(lua.lua_state());
    lua_setmetatable(state, -2);
    lua_pop(state, 1);
    registryTable(lua, MONITOR_STATES_KEY, "k").raw_set(target, monitor);
    return monitor;
}

void registerMonitor(lua_glue::ThisState state, const lua_glue::Object& target,
                     const std::string& name,
                     const lua_glue::Function& callback, lua_glue::Arguments) {
    lua_glue::StateView lua(state);
    if (!lua_isnoneornil(state, 4) && !lua_istable(state, 4)) {
        throw std::invalid_argument("Monitor params must be a table or nil");
    }
    if (!lua_isnoneornil(state, 5) && !lua_isboolean(state, 5)) {
        throw std::invalid_argument(
            "Monitor notifyEqualWrites must be a boolean or nil");
    }
    if (!lua_isnoneornil(state, 6) && lua_type(state, 6) != LUA_TSTRING) {
        throw std::invalid_argument(
            "Monitor identifier must be a string or nil");
    }
    const lua_glue::Table params =
        lua_isnoneornil(state, 4) ? lua.create_table()
                                  : lua_glue::Read<lua_glue::Table>(state, 4);
    const bool notifyEqualWrites = lua_toboolean(state, 5) != 0;
    const std::string identifier =
        lua_isnoneornil(state, 6) ? "" : lua_glue::Read<std::string>(state, 6);
    if (name.empty()) {
        throw std::invalid_argument("Monitor field name must not be empty");
    }
    if (target.get_type() == lua_glue::Type::Table) {
        lua_glue::Table object = target.as<lua_glue::Table>();
        lua_glue::Table monitor = monitorState(lua, target);
        if (!monitor.raw_get<lua_glue::Object>("fields")
                 .is<lua_glue::Table>()) {
            monitor = createTableMonitorState(lua, object);
        }
        lua_glue::Table fields = monitor.raw_get<lua_glue::Table>("fields");
        const lua_glue::Object rawEntry =
            fields.raw_get<lua_glue::Object>(name);
        if (rawEntry.is<lua_glue::Table>()) {
            registerMonitorCallback(lua, rawEntry.as<lua_glue::Table>(),
                                    callback, params, notifyEqualWrites,
                                    identifier);
            return;
        }
        const lua_glue::Object rawValue =
            object.raw_get<lua_glue::Object>(name);
        lua_glue::Object value = rawValue;
        if (!value.valid() || value.get_type() == lua_glue::Type::Nil) {
            value = originalMonitoredIndex(lua, monitor, target,
                                           lua_glue::MakeObject(lua, name));
        }
        lua_glue::Table entry = lua.create_table();
        const bool hasValue =
            value.valid() && value.get_type() != lua_glue::Type::Nil;
        entry.raw_set("hasValue", hasValue);
        if (hasValue) {
            entry.raw_set("value", value);
        }
        registerMonitorCallback(lua, entry, callback, params, notifyEqualWrites,
                                identifier);
        entry.raw_set("raw", rawValue.valid() &&
                                 rawValue.get_type() != lua_glue::Type::Nil);
        entry.raw_set("assigned", false);
        entry.raw_set("missing", monitorMissing(lua));
        fields.raw_set(name, entry);
        object.raw_set(name, lua_glue::nil);
        return;
    }
    if (target.get_type() != lua_glue::Type::Userdata) {
        throw std::invalid_argument(
            "Monitors require a table or userdata target");
    }
    lua_glue::Table fields = class_native::getUserFields(lua, target, true);
    const lua_glue::Object rawMonitors =
        fields.raw_get<lua_glue::Object>("__monitorCallbacks");
    lua_glue::Table monitors = rawMonitors.is<lua_glue::Table>()
                                   ? rawMonitors.as<lua_glue::Table>()
                                   : lua.create_table();
    if (!rawMonitors.is<lua_glue::Table>()) {
        fields.raw_set("__monitorCallbacks", monitors);
    }
    const lua_glue::Object rawEntry = monitors.raw_get<lua_glue::Object>(name);
    lua_glue::Table entry = rawEntry.is<lua_glue::Table>()
                                ? rawEntry.as<lua_glue::Table>()
                                : lua.create_table();
    registerMonitorCallback(lua, entry, callback, params, notifyEqualWrites,
                            identifier);
    entry.raw_set("missing", monitorMissing(lua));
    monitors.raw_set(name, entry);
}

void unregisterMonitor(lua_glue::ThisState state,
                       const lua_glue::Object& target, const std::string& name,
                       std::optional<std::string> identifier) {
    lua_glue::StateView lua(state);
    if (target.get_type() == lua_glue::Type::Table) {
        lua_glue::Table object = target.as<lua_glue::Table>();
        lua_glue::Table monitor = monitorState(lua, target);
        const lua_glue::Object rawFields =
            monitor.raw_get<lua_glue::Object>("fields");
        if (!rawFields.is<lua_glue::Table>()) {
            return;
        }
        lua_glue::Table fields = rawFields.as<lua_glue::Table>();
        const lua_glue::Object rawEntry =
            fields.raw_get<lua_glue::Object>(name);
        if (!rawEntry.is<lua_glue::Table>()) {
            return;
        }
        lua_glue::Table entry = rawEntry.as<lua_glue::Table>();
        if (!unregisterMonitorCallback(entry, identifier.value_or(""))) {
            return;
        }
        fields.raw_set(name, lua_glue::nil);
        const bool restore =
            rawBool(entry, "raw") || rawBool(entry, "assigned");
        if (restore) {
            const bool hasValue = rawBool(entry, "hasValue");
            object.raw_set(name, hasValue
                                     ? entry.raw_get<lua_glue::Object>("value")
                                     : nilObject(lua));
        }
        if (!tableIsEmpty(fields)) {
            return;
        }
        const lua_glue::Object originalMetatable =
            monitor.raw_get<lua_glue::Object>("meta");
        object.push(lua.lua_state());
        if (originalMetatable.valid() &&
            originalMetatable.get_type() != lua_glue::Type::Nil) {
            originalMetatable.push(lua.lua_state());
        } else {
            lua_pushnil(lua.lua_state());
        }
        lua_setmetatable(lua.lua_state(), -2);
        lua_pop(lua.lua_state(), 1);
        registryTable(lua, MONITOR_STATES_KEY, "k")
            .raw_set(object, lua_glue::nil);
        return;
    }
    if (target.get_type() != lua_glue::Type::Userdata) {
        return;
    }
    lua_glue::Table fields = class_native::getUserFields(lua, target, true);
    const lua_glue::Object rawCallbacks =
        fields.raw_get<lua_glue::Object>("__monitorCallbacks");
    if (!rawCallbacks.is<lua_glue::Table>()) {
        return;
    }
    lua_glue::Table callbacks = rawCallbacks.as<lua_glue::Table>();
    const lua_glue::Object rawEntry = callbacks.raw_get<lua_glue::Object>(name);
    if (!rawEntry.is<lua_glue::Table>() ||
        !unregisterMonitorCallback(rawEntry.as<lua_glue::Table>(),
                                   identifier.value_or(""))) {
        return;
    }
    callbacks.raw_set(name, lua_glue::nil);
    if (tableIsEmpty(callbacks)) {
        fields.raw_set("__monitorCallbacks", lua_glue::nil);
    }
}

}  // namespace ludork::standard::class_runtime::detail
