#include "Class/ClassRuntimeInternals.hpp"
#include <ClassRuntimeProtocol.hpp>

#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/TypeQueries.hpp"
#include "Detail/TypedFields.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassServices.hpp>
#include <LuaGlue/LuaGlue.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace ludork::standard::class_runtime {

namespace {

bool hasManagedField(lua_glue::StateView lua, const lua_glue::Object& target,
                     const lua_glue::Object& key) {
    if (target.is<lua_glue::Table>() &&
        detail::isClass(target.as<lua_glue::Table>())) {
        return false;
    }
    const lua_glue::Object rawClass = detail::scriptClassOf(lua, target);
    if (!rawClass.is<lua_glue::Table>()) {
        return target.get_type() == lua_glue::Type::Userdata;
    }
    const lua_glue::Table classTable = rawClass.as<lua_glue::Table>();
    if (detail::findAccessor(lua, classTable, protocol::CLASS_GETTERS_FIELD,
                             key)
            .is<lua_glue::Function>() ||
        detail::findAccessor(lua, classTable, protocol::CLASS_SETTERS_FIELD,
                             key)
            .is<lua_glue::Function>()) {
        return true;
    }
    if (target.get_type() != lua_glue::Type::Userdata) {
        return false;
    }
    for (const auto& entry : detail::getMro(lua, classTable)) {
        if (!entry.second.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table type = entry.second.as<lua_glue::Table>();
        if (!detail::isNativeType(lua, type)) {
            continue;
        }
        if (detail::nativeTypeDeclaresProperty(type, key)) {
            return true;
        }
        const lua_glue::Object member =
            detail::nativeTypeDefinition(lua, type, key);
        if (member.valid() && member.get_type() != lua_glue::Type::Nil) {
            return true;
        }
    }
    return false;
}

}  // namespace

lua_glue::Table finalizeClass(lua_glue::Table definition,
                              const lua_glue::Table& bases) {
    return detail::finalizeClassImpl(std::move(definition), bases);
}

lua_glue::Object protectedGet(lua_glue::StateView lua,
                              const lua_glue::Object& target,
                              const lua_glue::Object& key) {
    return detail::protectedIndex(lua, target, key);
}

void protectedSet(lua_glue::StateView lua, const lua_glue::Object& target,
                  const lua_glue::Object& key, const lua_glue::Object& value) {
    detail::protectedAssign(lua, target, key, value);
}

void protectedSetTyped(lua_glue::StateView lua, const lua_glue::Object& target,
                       const lua_glue::Object& key,
                       const lua_glue::Object& value) {
    if (!key.is<std::string>() ||
        (target.get_type() != lua_glue::Type::Table &&
         target.get_type() != lua_glue::Type::Userdata)) {
        throw std::invalid_argument(
            "Typed fields require an object and a string key");
    }
    detail::protectedAssign(lua, target, key, value);
    if ((!value.valid() || value.get_type() == lua_glue::Type::Nil) &&
        !hasManagedField(lua, target, key)) {
        detail::markExplicitNilField(lua, target, key);
    }
    if (target.is<lua_glue::Table>() &&
        detail::isClass(target.as<lua_glue::Table>())) {
        detail::invalidateClassLookup(lua, target.as<lua_glue::Table>());
    }
}

lua_glue::Object rawGetOwnField(lua_glue::StateView lua,
                                const lua_glue::Object& target,
                                const lua_glue::Object& key) {
    return detail::rawOwnField(lua, target, key);
}

bool hasOwnField(lua_glue::StateView lua, const lua_glue::Object& target,
                 const lua_glue::Object& key) {
    return detail::hasRawOwnField(lua, target, key);
}

lua_glue::Table getOwnKeys(lua_glue::StateView lua,
                           const lua_glue::Object& target) {
    return detail::ownKeyList(lua, target);
}

bool rawEqual(const lua_glue::Object& left, const lua_glue::Object& right) {
    return detail::objectsRawEqual(left, right);
}

lua_glue::Table getMroCopy(lua_glue::StateView lua,
                           const lua_glue::Object& value) {
    return detail::mroCopy(lua, value);
}

lua_glue::Object typeOf(lua_glue::StateView lua,
                        const lua_glue::Object& value) {
    if (value.is<lua_glue::Table>() &&
        detail::isClass(value.as<lua_glue::Table>())) {
        return lua.globals().raw_get<lua_glue::Object>("Class");
    }
    const lua_glue::Object result = detail::actualClassOf(lua, value);
    if (result.is<lua_glue::Table>()) {
        return result;
    }
    return lua_glue::MakeObject(
        lua, lua_glue::TypeName(lua.lua_state(), value.get_type()));
}

bool isInstanceOf(lua_glue::StateView lua, const lua_glue::Object& value,
                  const lua_glue::Table& targetClass) {
    const lua_glue::Object rawClass = detail::scriptClassOf(lua, value);
    if (rawClass.is<lua_glue::Table>()) {
        return detail::derivesFrom(lua, rawClass.as<lua_glue::Table>(),
                                   targetClass);
    }
    return value.get_type() == lua_glue::Type::Userdata &&
           detail::nativeTypeAccepts(lua, targetClass, value);
}

bool isSubclassOf(lua_glue::StateView lua, const lua_glue::Table& value,
                  const lua_glue::Table& targetClass) {
    return detail::derivesFrom(lua, value, targetClass);
}

lua_glue::Object requireModule(lua_glue::StateView lua,
                               const std::string& moduleName) {
    const lua_glue::Object rawRequire =
        lua.globals().raw_get<lua_glue::Object>("require");
    if (!rawRequire.is<lua_glue::Function>()) {
        throw std::runtime_error("Lua require function is not defined");
    }
    lua_glue::Function require = rawRequire.as<lua_glue::Function>();
    lua_glue::CallResult result = require(moduleName);
    if (!result.valid()) {
        const std::string error = result.error();
        throw std::runtime_error(error.c_str());
    }
    return result.return_count() == 0 ? lua_glue::MakeObject(lua, lua_glue::nil)
                                      : result.get<lua_glue::Object>();
}

}  // namespace ludork::standard::class_runtime
