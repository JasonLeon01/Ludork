#include "Instance/InstanceRuntime.hpp"

#include "Composite/CompositeRuntime.hpp"
#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Native/NativeRuntime.hpp"

#include <sol2/sol.hpp>

#include <stdexcept>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Instance validation
// ───────────────────────────────────────────────────────

void validateNativeInstanceShape(sol::state_view lua,
                                 const sol::table& classTable,
                                 const sol::object& instance) {
    const std::vector<sol::table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return;
    }
    if (!isCompositeInstance(lua, instance)) {
        throw std::runtime_error(
            "Class with native bases must return its composite instance");
    }
    const sol::table fields = class_native::getUserFields(lua, instance, false);
    const sol::object rawClass = fields.raw_get<sol::object>("__class");
    if (!rawClass.is<sol::table>() ||
        !objectsRawEqual(rawClass.as<sol::table>(), classTable)) {
        throw std::runtime_error("Composite instance belongs to another class");
    }
}

void validateNativeRoots(sol::state_view lua, const sol::table& classTable,
                         const sol::object& instance) {
    validateNativeInstanceShape(lua, classTable, instance);
    const std::vector<sol::table> roots = nativeRoots(lua, classTable);
    if (roots.empty()) {
        return;
    }
    const sol::table fields = class_native::getUserFields(lua, instance, false);
    for (const sol::table& root : roots) {
        if (!nativeObjectForType(lua, fields, root).is<sol::userdata>()) {
            throw std::runtime_error("Lua class initializer must call " +
                                     nativeTypeName(lua, root) +
                                     ".init(self, ...)");
        }
    }
}

bool compositeBelongsToClass(sol::state_view lua, const sol::object& instance,
                             const sol::table& classTable) {
    if (!isCompositeInstance(lua, instance)) {
        return false;
    }
    const sol::object rawClass =
        class_native::getUserFields(lua, instance, false)
            .raw_get<sol::object>("__class");
    return rawClass.is<sol::table>() &&
           objectsRawEqual(rawClass.as<sol::table>(), classTable);
}

}  // namespace ludork::standard::class_runtime::detail
