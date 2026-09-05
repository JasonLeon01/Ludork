#include "Class/ClassRuntimeInternals.hpp"

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassServices.hpp>
#include <sol2/sol.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace ludork::standard::class_runtime {

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
