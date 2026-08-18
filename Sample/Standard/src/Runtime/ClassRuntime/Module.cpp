#include "Internal.hpp"

#include <sol2/sol.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <climits>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Callable introspection ────────────────────────────────────────────────────

CallableInfo inspectCallable(const sol::object& callable) {
    CallableInfo result;
    if (callable.get_type() != sol::type::function) {
        return result;
    }
    lua_State* state = callable.lua_state();
    callable.push();
    lua_Debug info{};
    if (lua_getinfo(state, ">u", &info) == 0) {
        return result;
    }
    result.parameterCount = static_cast<int>(info.nparams);
    result.vararg = info.isvararg != 0;
    callable.push();
    result.parameterNames.reserve(info.nparams);
    for (int index = 1; index <= static_cast<int>(info.nparams); ++index) {
        const char* name = lua_getlocal(state, nullptr, index);
        if (name != nullptr && std::string(name) != "self") {
            result.parameterNames.emplace_back(name);
        }
    }
    lua_pop(state, 1);
    return result;
}

sol::table constructorClass(lua_State* state) {
    return sol::stack::get<sol::table>(state, lua_upvalueindex(1));
}

// ── Class construction entry points ──────────────────────────────────────────

namespace {

void callConstructorFunction(lua_State* state, const sol::object& function,
                             const sol::object& instance, int firstArgument,
                             int originalTop) {
    const CallableInfo info = inspectCallable(function);
    const int argumentCount =
        firstArgument <= originalTop ? originalTop - firstArgument + 1 : 0;
    const int maximumArgumentCount = std::max(0, info.parameterCount - 1);
    if (!info.vararg && argumentCount > maximumArgumentCount) {
        throw std::invalid_argument(
            "Class initializer received too many arguments");
    }
    ensureRuntimeLuaStack(
        state, static_cast<std::size_t>(argumentCount) + 2,
        "class initializer arguments");
    function.push();
    instance.push();
    for (int index = firstArgument; index <= originalTop; ++index) {
        lua_pushvalue(state, index);
    }
    if (ludork::standard::protectedLuaCall(
            state, originalTop - firstArgument + 2, 0) != LUA_OK) {
        size_t length = 0;
        const char* message = luaL_tolstring(state, -1, &length);
        const std::string error = message == nullptr
                                      ? "Class initializer failed"
                                      : std::string(message, length);
        lua_pop(state, 2);
        throw std::runtime_error(error);
    }
}

int constructClassInstance(lua_State* state, int firstArgument) {
    sol::state_view lua(state);
    const int originalTop = lua_gettop(state);
    const sol::table classTable = constructorClass(state);
    const sol::object initializer =
        findScriptMember(lua, classTable, sol::make_object(lua, "init"));
    const bool hasInitializer = initializer.is<sol::function>();
    sol::object instance;
    if (!hasInitializer && firstArgument <= originalTop) {
        const std::vector<sol::table> roots = nativeRoots(lua, classTable);
        if (roots.size() > 1) {
            throw std::invalid_argument(
                "Class with multiple native roots and constructor arguments "
                "must define init");
        }
        if (roots.size() == 1) {
            sol::table arguments = lua.create_table();
            const int argumentCount = originalTop - firstArgument + 1;
            arguments.raw_set("n", argumentCount);
            for (int index = firstArgument; index <= originalTop; ++index) {
                arguments.raw_set(index - firstArgument + 1,
                                  sol::stack::get<sol::object>(state, index));
            }
            sol::table constructorArguments = lua.create_table();
            constructorArguments.raw_set(roots.front(), arguments);
            instance =
                allocateInstance(lua, classTable, constructorArguments);
        } else {
            throw std::invalid_argument(
                "Class without init does not accept constructor arguments");
        }
    } else {
        instance =
            allocateInstance(lua, classTable, sol::object(), hasInitializer);
    }
    try {
        validateNativeInstanceShape(lua, classTable, instance);
        syncNativeClassDefaults(lua, classTable, instance);
        if (hasInitializer) {
            callConstructorFunction(state, initializer, instance, firstArgument,
                                    originalTop);
        }
        if (hasInitializer) {
            completeDefaultNativeRoots(lua, classTable, instance);
        }
        validateNativeRoots(lua, classTable, instance);
        finishNativeConstruction(lua, classTable, instance);
    } catch (...) {
        failNativeConstruction(lua, classTable, instance);
        throw;
    }
    instance.push();
    return 1;
}

}  // namespace

int classNew(lua_State* state) {
    try {
        return constructClassInstance(state, 1);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classCall(lua_State* state) {
    try {
        return constructClassInstance(state, 2);
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

// ── Class instance metamethods ────────────────────────────────────────────────

namespace {

int classInstanceIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        const sol::table classTable = constructorClass(state);
        const sol::object target = sol::stack::get<sol::object>(state, 1);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object getter =
            findAccessor(lua, classTable, "__getters", key);
        if (getter.is<sol::function>()) {
            getter.push();
            target.push();
            lua_call(state, 1, 1);
            return 1;
        }
        findInClass(lua, classTable, key).push();
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classInstanceNewIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        sol::table classTable = constructorClass(state);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        const sol::object setter =
            findAccessor(lua, classTable, "__setters", key);
        if (setter.is<sol::function>()) {
            setter.push();
            lua_pushvalue(state, 1);
            lua_pushvalue(state, 3);
            lua_call(state, 2, 0);
            return 0;
        }
        lua_pushvalue(state, 2);
        lua_pushvalue(state, 3);
        lua_rawset(state, 1);
        invalidateClassLookup(lua, classTable);
        const sol::object value = sol::stack::get<sol::object>(state, 3);
        if (value.is<sol::function>()) {
            const sol::object implementationOwner =
                classTable.raw_get<sol::object>("_hasImplementationOwner");
            if (implementationOwner.valid() &&
                implementationOwner.get_type() != sol::type::lua_nil) {
                classTable.raw_set("_hasImplementationOwner", sol::lua_nil);
            }
        }
        registerMethodOwner(lua, classTable, value);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classMetatableIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        const sol::object key = sol::stack::get<sol::object>(state, 2);
        findInClass(lua, constructorClass(state), key, false).push();
        return 1;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

int classMetatableNewIndex(lua_State* state) {
    try {
        sol::state_view lua(state);
        sol::table classTable = constructorClass(state);
        const sol::object value = sol::stack::get<sol::object>(state, 3);
        lua_pushvalue(state, 2);
        lua_pushvalue(state, 3);
        lua_rawset(state, 1);
        invalidateClassLookup(lua, classTable);
        if (value.is<sol::function>()) {
            const sol::object implementationOwner =
                classTable.raw_get<sol::object>("_hasImplementationOwner");
            if (implementationOwner.valid() &&
                implementationOwner.get_type() != sol::type::lua_nil) {
                classTable.raw_set("_hasImplementationOwner", sol::lua_nil);
            }
        }
        registerMethodOwner(lua, classTable, value);
        return 0;
    } catch (const std::exception& error) {
        return luaL_error(state, "%s", error.what());
    }
}

}  // namespace

void setClassClosure(lua_State* state, const sol::table& target,
                     const char* name, const sol::table& classTable,
                     lua_CFunction function) {
    target.push();
    lua_pushstring(state, name);
    classTable.push();
    lua_pushcclosure(state, function, 1);
    lua_rawset(state, -3);
    lua_pop(state, 1);
}

// ── Class finalization ────────────────────────────────────────────────────────

namespace {

bool isFinalizedClass(const sol::table& value) {
    return isClass(value) && tableHasMetatable(value) &&
           value.raw_get<sol::object>("__bases").is<sol::table>() &&
           value.raw_get<sol::object>("__mro").is<sol::table>() &&
           value.raw_get<sol::object>("__index").is<sol::function>() &&
           value.raw_get<sol::object>("__newindex").is<sol::function>() &&
           value.raw_get<sol::object>("new").is<sol::function>();
}

constexpr const char* CLASS_RESERVED_FIELDS[] = {
    "__ludorkClass",      "__name",
    "__bases",            "__base",
    "__mro",              "__mroSet",
    "__runtimeBases",     "__runtimeMro",
    "__runtimeMroSet",    "__nativeBases",
    "__nativeMro",        "__nativeMroSet",
    "__subclasses",       "__lookupCache",
    "__lookupVersion",    "__index",
    "__newindex",         "__gc",
    "__call",             "new",
    "_hasImplementationOwner",
    "__classBaseMethods", "__classCallbacks",
    "__classDefaults",    "__classResolvedDefaults",
    "__classFactory",     "__classFactoryMinArgs",
    "__classInit",        "__nativeMethodCache",
    "__nativeObjects",    "__nativeProperties",
};

void validateClassDefinition(const sol::table& definition) {
    if (rawBool(definition, "__ludorkClass")) {
        throw std::invalid_argument("Class definition is already finalized");
    }
    if (tableHasMetatable(definition)) {
        throw std::invalid_argument(
            "Class definition must be a plain table without a metatable");
    }
    for (const char* name : CLASS_RESERVED_FIELDS) {
        const sol::object value = definition.raw_get<sol::object>(name);
        if (value.valid() && value.get_type() != sol::type::lua_nil) {
            throw std::invalid_argument(
                "Class definition contains reserved field '" +
                std::string(name) + "'");
        }
    }
}

sol::table normalizeClassBases(sol::state_view lua, const sol::table& bases) {
    sol::table result = lua.create_table();
    std::vector<sol::table> accepted;
    accepted.reserve(bases.size());
    for (std::size_t index = 1; index <= bases.size(); ++index) {
        const sol::object rawBase = bases.raw_get<sol::object>(index);
        if (!rawBase.is<sol::table>()) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        const sol::table base = rawBase.as<sol::table>();
        if (!isFinalizedClass(base) && !isNativeType(lua, base)) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        bool duplicate = false;
        for (const sol::table& existing : accepted) {
            if (objectsRawEqual(existing, base)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            accepted.push_back(base);
            result.add(base);
        }
    }
    return result;
}

}  // namespace

sol::table finalizeClassImpl(sol::table definition, const sol::table& bases) {
    sol::state_view lua(definition.lua_state());
    validateClassDefinition(definition);
    const sol::table baseList = normalizeClassBases(lua, bases);
    const std::vector<sol::table> linearization =
        createMro(definition, baseList, MroKind::Runtime);
    for (std::size_t index = 1; index < linearization.size(); ++index) {
        if (isNativeType(lua, linearization[index])) {
            ensureNativeInitializer(lua, linearization[index]);
        }
    }
    std::vector<sol::object> ownMethods;
    for (const auto& entry : definition) {
        if (entry.second.is<sol::function>()) {
            ownMethods.push_back(entry.second);
        }
    }
    sol::table mro = lua.create_table();
    for (const sol::table& type : linearization) {
        mro.add(type);
    }
    sol::table classTable = definition;
    classTable.raw_set("__ludorkClass", true);
    classTable.raw_set("__lookupVersion", 1);
    classTable.raw_set("__bases", baseList);
    if (baseList.size() > 0) {
        classTable.raw_set("__base", baseList[1]);
    }
    classTable.raw_set("__mro", mro);
    ensureMroSet(lua, classTable, mro, "__mroSet");
    for (const sol::table& base : tableList(baseList)) {
        registerSubclass(lua, base, classTable);
    }
    setClassClosure(lua.lua_state(), classTable, "__index", classTable,
                    classInstanceIndex);
    setClassClosure(lua.lua_state(), classTable, "__newindex", classTable,
                    classInstanceNewIndex);
    setClassClosure(lua.lua_state(), classTable, "__gc", classTable,
                    classInstanceGc);
    setClassClosure(lua.lua_state(), classTable, "new", classTable, classNew);
    sol::table classMetatable = lua.create_table();
    setClassClosure(lua.lua_state(), classMetatable, "__index", classTable,
                    classMetatableIndex);
    setClassClosure(lua.lua_state(), classMetatable, "__newindex", classTable,
                    classMetatableNewIndex);
    setClassClosure(lua.lua_state(), classMetatable, "__call", classTable,
                    classCall);
    classTable[sol::metatable_key] = classMetatable;
    for (const sol::object& method : ownMethods) {
        registerMethodOwner(lua, classTable, method);
    }
    return classTable;
}

namespace {

sol::table classFunction(sol::this_state state, const sol::object& definition,
                         sol::variadic_args bases) {
    sol::state_view lua(state);
    if (!definition.is<sol::table>()) {
        throw std::invalid_argument("Class definition must be a table");
    }
    sol::table baseList = lua.create_table();
    for (const sol::stack_proxy& rawBase : bases) {
        const sol::object base = sol::make_object(lua, rawBase);
        if (!base.is<sol::table>()) {
            throw std::invalid_argument(
                "Class bases must be finalized class tables or native types");
        }
        baseList.add(base);
    }
    return finalizeClassImpl(definition.as<sol::table>(), baseList);
}

// ── Own-field accessors ───────────────────────────────────────────────────────

}  // namespace

sol::table ownFields(sol::state_view lua, const sol::object& target) {
    if (target.is<sol::table>()) {
        return target.as<sol::table>();
    }
    if (target.get_type() == sol::type::userdata) {
        return class_native::getUserFields(lua, target, false);
    }
    return lua.create_table();
}

sol::object rawOwnField(sol::state_view lua, const sol::object& target,
                        const sol::object& key) {
    if (target.get_type() == sol::type::userdata) {
        lua_State* state = lua.lua_state();
        target.push();
        if (lua_getiuservalue(state, -1, 1) != LUA_TTABLE) {
            lua_pop(state, 2);
            return nilObject(lua);
        }
        key.push();
        lua_rawget(state, -2);
        sol::object result = sol::stack::get<sol::object>(state, -1);
        lua_pop(state, 3);
        return result;
    }
    if (target.is<sol::table>()) {
        return target.as<sol::table>().raw_get<sol::object>(key);
    }
    return nilObject(lua);
}

bool hasRawOwnField(sol::state_view lua, const sol::object& target,
                    const sol::object& key) {
    const sol::object value = rawOwnField(lua, target, key);
    return value.valid() && value.get_type() != sol::type::lua_nil;
}

sol::table ownKeyList(sol::state_view lua, const sol::object& target) {
    sol::table result = lua.create_table();
    for (const auto& entry : ownFields(lua, target)) {
        result.add(entry.first);
    }
    return result;
}

sol::table mroCopy(sol::state_view lua, const sol::object& value) {
    sol::object rawClass = value;
    if (!value.is<sol::table>() ||
        (!isClass(value.as<sol::table>()) &&
         !isNativeType(lua, value.as<sol::table>()))) {
        rawClass = actualClassOf(lua, value);
    }
    if (!rawClass.is<sol::table>()) {
        throw std::invalid_argument(
            "Class.getMro requires a class or class instance");
    }
    const sol::table mro = getMro(lua, rawClass.as<sol::table>());
    sol::table result = lua.create_table();
    for (std::size_t index = 1; index <= mro.size(); ++index) {
        result.add(mro.raw_get<sol::object>(index));
    }
    return result;
}

// ── Module-level functions (exposed on Class table) ───────────────────────────

namespace {

sol::table getParameterNames(const sol::object& callable) {
    sol::state_view lua(callable.lua_state());
    if (callable.get_type() != sol::type::function) {
        throw std::invalid_argument(
            "Class.getParameterNames requires a function");
    }
    const CallableInfo info = inspectCallable(callable);
    sol::table result = lua.create_table();
    for (const std::string& name : info.parameterNames) {
        result.add(name);
    }
    return result;
}

sol::object constructNamed(sol::this_state state, const sol::object& rawType,
                           const sol::object& rawArguments) {
    sol::state_view lua(state);
    if (!rawType.is<sol::table>()) {
        throw std::invalid_argument(
            "Class.constructNamed requires a class type");
    }
    sol::table arguments = lua.create_table();
    if (rawArguments.valid() && rawArguments.get_type() != sol::type::lua_nil) {
        if (!rawArguments.is<sol::table>()) {
            throw std::invalid_argument(
                "Class.constructNamed arguments must be a table");
        }
        arguments = rawArguments.as<sol::table>();
    }
    const sol::table type = rawType.as<sol::table>();
    sol::object initializer = nilObject(lua);
    if (isClass(type)) {
        initializer =
            findScriptMember(lua, type, sol::make_object(lua, "init"));
    } else {
        initializer = rawMember(lua, type, sol::make_object(lua, "init"));
    }
    std::vector<sol::object> values;
    if (initializer.get_type() == sol::type::function) {
        const CallableInfo info = inspectCallable(initializer);
        if (info.parameterNames.empty() && !tableIsEmpty(arguments)) {
            throw std::invalid_argument(
                "Class initializer exposes no named parameters");
        }
        values.reserve(info.parameterNames.size());
        for (const std::string& name : info.parameterNames) {
            const sol::object value =
                protectedIndex(lua, sol::make_object(lua, arguments),
                               sol::make_object(lua, name));
            values.push_back(value.valid() ? value : nilObject(lua));
        }
    } else if (!tableIsEmpty(arguments)) {
        throw std::invalid_argument(
            "Class without init does not accept named arguments");
    }
    const sol::object rawConstructor =
        protectedIndex(lua, rawType, sol::make_object(lua, "new"));
    if (!rawConstructor.is<sol::protected_function>()) {
        throw std::runtime_error("Class type has no new constructor");
    }
    lua_State* luaState = lua.lua_state();
    const int stackBase = lua_gettop(luaState);
    try {
        const int resultCount = invokeRuntimeFunction(
            lua, rawConstructor, values, "named constructor arguments");
        sol::object result =
            resultCount == 0
                ? nilObject(lua)
                : sol::stack::get<sol::object>(luaState, stackBase + 1);
        lua_settop(luaState, stackBase);
        return result;
    } catch (...) {
        lua_settop(luaState, stackBase);
        throw;
    }
}

bool isSubclass(sol::this_state state, const sol::table& value,
                const sol::table& targetClass) {
    return ludork::standard::class_runtime::isSubclassOf(
        sol::state_view(state), value, targetClass);
}

bool isInstance(sol::this_state state, const sol::object& value,
                const sol::table& targetClass) {
    return ludork::standard::class_runtime::isInstanceOf(
        sol::state_view(state), value, targetClass);
}

sol::object classType(sol::this_state state, const sol::object& value) {
    return ludork::standard::class_runtime::typeOf(sol::state_view(state),
                                                   value);
}

bool hasOwnFieldFunction(sol::this_state state, const sol::object& target,
                         const sol::object& key) {
    return hasRawOwnField(sol::state_view(state), target, key);
}

sol::table getMroFunction(sol::this_state state, const sol::object& value) {
    return mroCopy(sol::state_view(state), value);
}

sol::object copyFunction(sol::this_state state, const sol::object& value) {
    return class_runtime::shallowCopy(sol::state_view(state), value);
}

sol::object deepCopyFunction(sol::this_state state, const sol::object& value) {
    return class_runtime::deepCopy(sol::state_view(state), value);
}

}  // namespace

// ── Module entry point ────────────────────────────────────────────────────────

sol::table createModule(sol::state_view lua) {
    lua.registry().raw_set(SHUTTING_DOWN_KEY, sol::lua_nil);
    sol::table root = lua.create_table();
    root.set_function("isInstance", &isInstance);
    root.set_function("isSubclass", &isSubclass);
    root.set_function("type", &classType);
    root.set_function("hasOwnField", &hasOwnFieldFunction);
    root.set_function("getMro", &getMroFunction);
    root.set_function("getParameterNames", &getParameterNames);
    root.set_function("constructNamed", &constructNamed);
    root.set_function("super", superFunction);
    root.set_function("monitor", &registerMonitor);
    root.set_function("unmonitor", &unregisterMonitor);
    root.set_function("registerService", &detail::registerRuntimeService);
    root.set_function("unregisterService", &detail::unregisterRuntimeService);
    root.raw_set("MISSING", lua.create_table());
    lua_pushcfunction(lua.lua_state(), &detail::runtimeClassResolver);
    lua_setglobal(lua.lua_state(), "_LUDORK_RUNTIME_RESOLVER");
    lua.globals().set_function("class", &classFunction);
    lua.globals().set_function("copy", &copyFunction);
    lua.globals().set_function("deepcopy", &deepCopyFunction);
    lua["super"] = root["super"];
    return root;
}

// ── ClassRuntimeInternal.hpp detail interface ─────────────────────────────────

sol::table resolverMro(sol::state_view lua, const sol::table& classTable) {
    return getMro(lua, classTable);
}

}  // namespace ludork::standard::class_runtime::detail

// ── class_runtime:: public API ────────────────────────────────────────────────

namespace ludork::standard::class_runtime {

sol::table createModule(sol::state_view lua) {
    return detail::createModule(lua);
}

void shutdown(lua_State* state) noexcept {
    using namespace detail;
    if (state == nullptr) {
        return;
    }
    const int stackTop = lua_gettop(state);
    lua_pushboolean(state, 1);
    lua_setfield(state, LUA_REGISTRYINDEX, SHUTTING_DOWN_KEY);
    constexpr const char* registryKeys[] = {
        METHOD_OWNERS_KEY,
        NATIVE_TYPE_CACHE_KEY,
        INSTANCES_KEY,
        COMPOSITE_METATABLE_KEY,
        CONSTRUCTING_COMPOSITE_METATABLE_KEY,
        NATIVE_OWNERS_KEY,
        NATIVE_POINTER_OWNERS_KEY,
        DYNAMIC_NATIVE_WRITERS_KEY,
        SUPER_PROXY_CACHE_KEY,
        SUPER_PROXY_METATABLE_KEY,
        MONITOR_STATES_KEY,
        detail::RUNTIME_SERVICES_KEY,
        LIFECYCLE_STATES_KEY,
        DISPOSED_METATABLE_KEY,
        "LuaSF.JsonNullSentinel",
        "LuaSF.JsonArrayMetatable",
        "LuaSF.JsonEmptyArrayMetatable",
    };
    for (const char* key : registryKeys) {
        lua_pushnil(state);
        lua_setfield(state, LUA_REGISTRYINDEX, key);
    }
    lua_pushlightuserdata(
        state, static_cast<void*>(&nativeDeepCopyProtocolsKeyStorage));
    lua_pushnil(state);
    lua_rawset(state, LUA_REGISTRYINDEX);
    constexpr const char* globalKeys[] = {
        "Class",
        "class",
        "copy",
        "deepcopy",
        "super",
        "_LUDORK_RUNTIME_RESOLVER",
        "_LUDORK_STANDARD_UPDATE",
    };
    for (const char* key : globalKeys) {
        lua_pushnil(state);
        lua_setglobal(state, key);
    }
    lua_settop(state, stackTop);
}

sol::table finalizeClass(sol::table definition, const sol::table& bases) {
    return detail::finalizeClassImpl(std::move(definition), bases);
}

sol::object protectedGet(sol::state_view lua, const sol::object& target,
                         const sol::object& key) {
    return detail::protectedIndex(lua, target, key);
}

void protectedSet(sol::state_view lua, const sol::object& target,
                  const sol::object& key, const sol::object& value) {
    detail::protectedAssign(lua, target, key, value);
}

sol::object rawGetOwnField(sol::state_view lua, const sol::object& target,
                           const sol::object& key) {
    return detail::rawOwnField(lua, target, key);
}

bool hasOwnField(sol::state_view lua, const sol::object& target,
                 const sol::object& key) {
    return detail::hasRawOwnField(lua, target, key);
}

sol::table getOwnKeys(sol::state_view lua, const sol::object& target) {
    return detail::ownKeyList(lua, target);
}

bool rawEqual(const sol::object& left, const sol::object& right) {
    return detail::objectsRawEqual(left, right);
}

sol::table getMroCopy(sol::state_view lua, const sol::object& value) {
    return detail::mroCopy(lua, value);
}

sol::object typeOf(sol::state_view lua, const sol::object& value) {
    if (value.is<sol::table>() &&
        detail::isClass(value.as<sol::table>())) {
        return lua.globals().raw_get<sol::object>("Class");
    }
    const sol::object result = detail::actualClassOf(lua, value);
    if (result.is<sol::table>()) {
        return result;
    }
    return sol::make_object(lua,
                            sol::type_name(lua.lua_state(), value.get_type()));
}

bool isInstanceOf(sol::state_view lua, const sol::object& value,
                  const sol::table& targetClass) {
    const sol::object rawClass = detail::scriptClassOf(lua, value);
    if (rawClass.is<sol::table>()) {
        return detail::derivesFrom(lua, rawClass.as<sol::table>(), targetClass);
    }
    return value.get_type() == sol::type::userdata &&
           detail::nativeTypeAccepts(lua, targetClass, value);
}

bool isSubclassOf(sol::state_view lua, const sol::table& value,
                  const sol::table& targetClass) {
    return detail::derivesFrom(lua, value, targetClass);
}

sol::object requireModule(sol::state_view lua, const std::string& moduleName) {
    const sol::object rawRequire =
        lua.globals().raw_get<sol::object>("require");
    if (!rawRequire.is<sol::protected_function>()) {
        throw std::runtime_error("Lua require function is not defined");
    }
    sol::protected_function require = rawRequire.as<sol::protected_function>();
    sol::protected_function_result result = require(moduleName);
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(error.what());
    }
    return result.return_count() == 0
               ? sol::make_object(lua, sol::lua_nil)
               : result.get<sol::object>();
}

}  // namespace ludork::standard::class_runtime
