#include "Class/ClassRuntimeInternals.hpp"
#include <ClassRuntimeProtocol.hpp>

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/TypeQueries.hpp"
#include "Detail/TypedFields.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassServices.hpp>
#include <sol2/sol.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace ludork::standard::class_runtime {

namespace {

bool hasManagedField(sol::state_view lua, const sol::object& target,
                     const sol::object& key) {
    if (target.is<sol::table>() && detail::isClass(target.as<sol::table>())) {
        return false;
    }
    const sol::object rawClass = detail::scriptClassOf(lua, target);
    if (!rawClass.is<sol::table>()) {
        return target.get_type() == sol::type::userdata;
    }
    const sol::table classTable = rawClass.as<sol::table>();
    if (detail::findAccessor(lua, classTable, protocol::CLASS_GETTERS_FIELD,
                             key)
            .is<sol::function>() ||
        detail::findAccessor(lua, classTable, protocol::CLASS_SETTERS_FIELD,
                             key)
            .is<sol::function>()) {
        return true;
    }
    if (target.get_type() != sol::type::userdata) {
        return false;
    }
    for (const auto& entry : detail::getMro(lua, classTable)) {
        if (!entry.second.is<sol::table>()) {
            continue;
        }
        const sol::table type = entry.second.as<sol::table>();
        if (!detail::isNativeType(lua, type)) {
            continue;
        }
        if (detail::nativeTypeDeclaresProperty(type, key)) {
            return true;
        }
        const sol::object member = detail::nativeTypeDefinition(lua, type, key);
        if (member.valid() && member.get_type() != sol::type::lua_nil) {
            return true;
        }
    }
    return false;
}

}  // namespace

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

void protectedSetTyped(sol::state_view lua, const sol::object& target,
                       const sol::object& key, const sol::object& value) {
    if (!key.is<std::string>() || (target.get_type() != sol::type::table &&
                                   target.get_type() != sol::type::userdata)) {
        throw std::invalid_argument(
            "Typed fields require an object and a string key");
    }
    detail::protectedAssign(lua, target, key, value);
    if ((!value.valid() || value.get_type() == sol::type::lua_nil) &&
        !hasManagedField(lua, target, key)) {
        detail::markExplicitNilField(lua, target, key);
    }
    if (target.is<sol::table>() && detail::isClass(target.as<sol::table>())) {
        detail::invalidateClassLookup(lua, target.as<sol::table>());
    }
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
    if (value.is<sol::table>() && detail::isClass(value.as<sol::table>())) {
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
    return result.return_count() == 0 ? sol::make_object(lua, sol::lua_nil)
                                      : result.get<sol::object>();
}

}  // namespace ludork::standard::class_runtime
