#include "Instance/InstanceRuntime.hpp"
#include "Detail/RuntimeState.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/TypeQueries.hpp"
#include "Native/NativeRuntime.hpp"

#include <LuaGlue/LuaGlue.hpp>

#include <stdexcept>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Instance validation
// ───────────────────────────────────────────────────────

void validateNativeInstanceShape(lua_glue::StateView lua,
                                 const lua_glue::Table& classTable,
                                 const lua_glue::Object& instance) {
    const std::vector<lua_glue::Table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return;
    }
    if (!isCompositeInstance(lua, instance)) {
        throw std::runtime_error(
            "Class with native bases must return its composite instance");
    }
    const lua_glue::Table fields =
        class_native::getUserFields(lua, instance, false);
    const lua_glue::Object rawClass =
        fields.raw_get<lua_glue::Object>(CLASS_FIELD);
    if (!rawClass.is<lua_glue::Table>() ||
        !objectsRawEqual(rawClass.as<lua_glue::Table>(), classTable)) {
        throw std::runtime_error("Composite instance belongs to another class");
    }
}

void validateNativeRoots(lua_glue::StateView lua,
                         const lua_glue::Table& classTable,
                         const lua_glue::Object& instance) {
    validateNativeInstanceShape(lua, classTable, instance);
    const std::vector<lua_glue::Table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return;
    }
    const lua_glue::Table fields =
        class_native::getUserFields(lua, instance, false);
    for (const lua_glue::Table& root : roots) {
        if ((nativeObjectForType(lua, fields, root).get_type() !=
             lua_glue::Type::Userdata)) {
            throw std::runtime_error("Lua class initializer must call " +
                                     nativeTypeName(lua, root) +
                                     ".init(self, ...)");
        }
    }
}

bool compositeBelongsToClass(lua_glue::StateView lua,
                             const lua_glue::Object& instance,
                             const lua_glue::Table& classTable) {
    if (!isCompositeInstance(lua, instance)) {
        return false;
    }
    const lua_glue::Object rawClass =
        class_native::getUserFields(lua, instance, false)
            .raw_get<lua_glue::Object>(CLASS_FIELD);
    return rawClass.is<lua_glue::Table>() &&
           objectsRawEqual(rawClass.as<lua_glue::Table>(), classTable);
}

}  // namespace ludork::standard::class_runtime::detail
