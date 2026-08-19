#include "Composite/CompositeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Native/NativeRuntime.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <sol2/sol.hpp>

#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Native property write helpers
// ─────────────────────────────────────────────

bool setNativeObjectMember(sol::state_view lua, const sol::object& nativeObject,
                           const sol::table& nativeType, const sol::object& key,
                           const sol::object& value) {
    if (!nativeObject.is<sol::userdata>()) {
        return false;
    }
    const bool declaredProperty = nativeTypeDeclaresProperty(nativeType, key);
    if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
        return false;
    }
    const sol::object nativeDefinition =
        nativeTypeDefinition(lua, nativeType, key);
    if (!nativeDefinition.valid() ||
        nativeDefinition.get_type() == sol::type::lua_nil) {
        return false;
    }
    if (!declaredProperty && !nativeDefinition.is<sol::function>()) {
        return false;
    }
    const sol::object current = protectedIndex(lua, nativeObject, key);
    if (!declaredProperty && current.is<sol::function>()) {
        return false;
    }
    protectedAssign(lua, nativeObject, key, value);
    return true;
}

bool setNativeMember(sol::state_view lua, const sol::table& fields,
                     const sol::table& classTable, const sol::object& key,
                     const sol::object& value, sol::object* assignedObject) {
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro[index];
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const sol::object nativeObject =
            nativeObjectForType(lua, fields, nativeType);
        if (setNativeObjectMember(lua, nativeObject, nativeType, key, value)) {
            if (assignedObject != nullptr) {
                *assignedObject = nativeObject;
            }
            return true;
        }
    }
    return false;
}

void markNativePropertyDirty(sol::state_view lua, sol::table fields,
                             const sol::object& nativeObject,
                             const sol::object& key) {
    if (!rawBool(fields, NATIVE_INITIALIZING_FIELD)) {
        return;
    }
    const sol::object rawDirty =
        fields.raw_get<sol::object>(NATIVE_DIRTY_PROPERTIES_FIELD);
    sol::table dirty = rawDirty.is<sol::table>() ? rawDirty.as<sol::table>()
                                                 : lua.create_table();
    if (!rawDirty.is<sol::table>()) {
        fields.raw_set(NATIVE_DIRTY_PROPERTIES_FIELD, dirty);
    }
    const sol::object rawProperties = dirty.raw_get<sol::object>(nativeObject);
    sol::table properties = rawProperties.is<sol::table>()
                                ? rawProperties.as<sol::table>()
                                : lua.create_table();
    if (!rawProperties.is<sol::table>()) {
        dirty.raw_set(nativeObject, properties);
    }
    properties.raw_set(key, true);
}

void restoreNativeShadows(sol::table fields,
                          const NativeShadowSnapshot& snapshot) {
    for (auto iterator = snapshot.rbegin(); iterator != snapshot.rend();
         ++iterator) {
        fields.raw_set(iterator->first, iterator->second);
    }
}

void syncNativeRootDefaults(sol::state_view lua, const sol::table& classTable,
                            const sol::object& instance, const sol::table& root,
                            const sol::object& nativeObject,
                            NativeShadowSnapshot& shadowSnapshot) {
    sol::table fields = class_native::getUserFields(lua, instance, false);
    std::vector<std::string> properties;
    std::unordered_set<std::string> seenProperties;
    const sol::table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const sol::object rawType = nativeMro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::object rawProperties =
            rawType.as<sol::table>().raw_get<sol::object>("__nativeProperties");
        if (!rawProperties.is<sol::table>()) {
            continue;
        }
        const sol::table nativeProperties = rawProperties.as<sol::table>();
        for (std::size_t propertyIndex = 1;
             propertyIndex <= nativeProperties.size(); ++propertyIndex) {
            const sol::object rawName =
                nativeProperties.raw_get<sol::object>(propertyIndex);
            if (rawName.is<std::string>()) {
                const std::string name = rawName.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    const sol::table classMro = getMro(lua, classTable);
    for (std::size_t index = 1; index <= classMro.size(); ++index) {
        const sol::object rawType = classMro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>() || !isClass(rawType.as<sol::table>())) {
            continue;
        }
        for (const auto& entry : rawType.as<sol::table>()) {
            if (entry.first.is<std::string>() &&
                nativeFallbackMemberEligible(entry.first)) {
                const std::string name = entry.first.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    for (const std::string& property : properties) {
        const sol::object key = sol::make_object(lua, property);
        const sol::object shadow = fields.raw_get<sol::object>(key);
        const bool hasShadow =
            shadow.valid() && shadow.get_type() != sol::type::lua_nil;
        const sol::object value =
            hasShadow ? shadow : findClassOverride(lua, classTable, key);
        if (!value.valid() || value.get_type() == sol::type::lua_nil) {
            continue;
        }
        bool assigned = false;
        for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
            const sol::object rawType = nativeMro.raw_get<sol::object>(index);
            if (!rawType.is<sol::table>()) {
                continue;
            }
            if (setNativeObjectMember(lua, nativeObject,
                                      rawType.as<sol::table>(), key, value)) {
                assigned = true;
                break;
            }
        }
        if (assigned && hasShadow) {
            shadowSnapshot.emplace_back(key, shadow);
            fields.raw_set(key, sol::lua_nil);
        }
    }
}

void replayNativeDirtyProperties(sol::state_view lua, const sol::table& fields,
                                 const sol::table& root,
                                 const sol::object& source,
                                 const sol::object& destination) {
    if (!source.is<sol::userdata>() || !destination.is<sol::userdata>()) {
        return;
    }
    const sol::object rawDirty =
        fields.raw_get<sol::object>(NATIVE_DIRTY_PROPERTIES_FIELD);
    if (!rawDirty.is<sol::table>()) {
        return;
    }
    const sol::object rawProperties =
        rawDirty.as<sol::table>().raw_get<sol::object>(source);
    if (!rawProperties.is<sol::table>()) {
        return;
    }
    const sol::table nativeMro = getNativeMro(lua, root);
    for (const auto& entry : rawProperties.as<sol::table>()) {
        if (!entry.second.is<bool>() || !entry.second.as<bool>()) {
            continue;
        }
        const sol::object key = entry.first;
        const sol::object value = protectedIndex(lua, source, key);
        for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
            const sol::object rawType = nativeMro.raw_get<sol::object>(index);
            if (rawType.is<sol::table>() &&
                setNativeObjectMember(lua, destination,
                                      rawType.as<sol::table>(), key, value)) {
                break;
            }
        }
    }
}

void syncNativeClassDefaults(sol::state_view lua, const sol::table& classTable,
                             const sol::object& instance) {
    if (instance.get_type() != sol::type::userdata) {
        return;
    }
    const sol::table fields = class_native::getUserFields(lua, instance, false);
    if (!fields.raw_get<sol::object>(protocol::NATIVE_OBJECTS_FIELD)
             .is<sol::table>()) {
        return;
    }
    std::vector<std::string> properties;
    std::unordered_set<std::string> seenProperties;
    const sol::table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const sol::object rawType = mro.raw_get<sol::object>(index);
        if (!rawType.is<sol::table>()) {
            continue;
        }
        const sol::table nativeType = rawType.as<sol::table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const sol::object rawProperties =
            nativeType.raw_get<sol::object>("__nativeProperties");
        if (!rawProperties.is<sol::table>()) {
            continue;
        }
        const sol::table nativeProperties = rawProperties.as<sol::table>();
        for (std::size_t propertyIndex = 1;
             propertyIndex <= nativeProperties.size(); ++propertyIndex) {
            const sol::object rawName =
                nativeProperties.raw_get<sol::object>(propertyIndex);
            if (rawName.is<std::string>()) {
                const std::string name = rawName.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    for (const std::string& property : properties) {
        const sol::object key = sol::make_object(lua, property);
        const sol::object value = findClassOverride(lua, classTable, key);
        if (value.valid() && value.get_type() != sol::type::lua_nil) {
            setNativeMember(lua, fields, classTable, key, value);
        }
    }
}

}  // namespace ludork::standard::class_runtime::detail
