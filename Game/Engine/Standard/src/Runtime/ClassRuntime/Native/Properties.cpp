#include "Native/NativeRuntime.hpp"

#include "Detail/ClassNativeInterop.hpp"
#include "Detail/Hierarchy.hpp"
#include "Detail/LuaSupport.hpp"
#include "Detail/RuntimeState.hpp"
#include "Detail/TypeQueries.hpp"

#include <ClassRuntimeProtocol.hpp>
#include <LuaGlue/LuaGlue.hpp>

#include <cstddef>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::standard::class_runtime::detail {

// ── Native property write helpers
// ─────────────────────────────────────────────

bool setNativeObjectMember(lua_glue::StateView lua,
                           const lua_glue::Object& nativeObject,
                           const lua_glue::Table& nativeType,
                           const lua_glue::Object& key,
                           const lua_glue::Object& value) {
    if ((nativeObject.get_type() != lua_glue::Type::Userdata)) {
        return false;
    }
    const bool declaredProperty = nativeTypeDeclaresProperty(nativeType, key);
    if (!declaredProperty && !nativeFallbackMemberEligible(key)) {
        return false;
    }
    const lua_glue::Object nativeDefinition =
        nativeTypeDefinition(lua, nativeType, key);
    if (!nativeDefinition.valid() ||
        nativeDefinition.get_type() == lua_glue::Type::Nil) {
        return false;
    }
    if (!declaredProperty && !nativeDefinition.is<lua_glue::Function>()) {
        return false;
    }
    const lua_glue::Object current = protectedIndex(lua, nativeObject, key);
    if (!declaredProperty && current.is<lua_glue::Function>()) {
        return false;
    }
    protectedAssign(lua, nativeObject, key, value);
    return true;
}

bool setNativeMember(lua_glue::StateView lua, const lua_glue::Table& fields,
                     const lua_glue::Table& classTable,
                     const lua_glue::Object& key, const lua_glue::Object& value,
                     lua_glue::Object* assignedObject) {
    const lua_glue::Table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro[index];
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table nativeType = rawType.as<lua_glue::Table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const lua_glue::Object nativeObject =
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

void markNativePropertyDirty(lua_glue::StateView lua, lua_glue::Table fields,
                             const lua_glue::Object& nativeObject,
                             const lua_glue::Object& key) {
    if (!rawBool(fields, NATIVE_INITIALIZING_FIELD)) {
        return;
    }
    const lua_glue::Object rawDirty =
        fields.raw_get<lua_glue::Object>(NATIVE_DIRTY_PROPERTIES_FIELD);
    lua_glue::Table dirty = rawDirty.is<lua_glue::Table>()
                                ? rawDirty.as<lua_glue::Table>()
                                : lua.create_table();
    if (!rawDirty.is<lua_glue::Table>()) {
        fields.raw_set(NATIVE_DIRTY_PROPERTIES_FIELD, dirty);
    }
    const lua_glue::Object rawProperties =
        dirty.raw_get<lua_glue::Object>(nativeObject);
    lua_glue::Table properties = rawProperties.is<lua_glue::Table>()
                                     ? rawProperties.as<lua_glue::Table>()
                                     : lua.create_table();
    if (!rawProperties.is<lua_glue::Table>()) {
        dirty.raw_set(nativeObject, properties);
    }
    properties.raw_set(key, true);
}

void restoreNativeShadows(lua_glue::Table fields,
                          const NativeShadowSnapshot& snapshot) {
    for (auto iterator = snapshot.rbegin(); iterator != snapshot.rend();
         ++iterator) {
        fields.raw_set(iterator->first, iterator->second);
    }
}

void syncNativeRootDefaults(lua_glue::StateView lua,
                            const lua_glue::Table& classTable,
                            const lua_glue::Object& instance,
                            const lua_glue::Table& root,
                            const lua_glue::Object& nativeObject,
                            NativeShadowSnapshot& shadowSnapshot) {
    lua_glue::Table fields = class_native::getUserFields(lua, instance, false);
    std::vector<std::string> properties;
    std::unordered_set<std::string> seenProperties;
    const lua_glue::Table nativeMro = getNativeMro(lua, root);
    for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
        const lua_glue::Object rawType =
            nativeMro.raw_get<lua_glue::Object>(index);
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Object rawProperties =
            rawType.as<lua_glue::Table>().raw_get<lua_glue::Object>(
                protocol::NATIVE_PROPERTIES_FIELD);
        if (!rawProperties.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table nativeProperties =
            rawProperties.as<lua_glue::Table>();
        for (std::size_t propertyIndex = 1;
             propertyIndex <= nativeProperties.size(); ++propertyIndex) {
            const lua_glue::Object rawName =
                nativeProperties.raw_get<lua_glue::Object>(propertyIndex);
            if (rawName.is<std::string>()) {
                const std::string name = rawName.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    const lua_glue::Table classMro = getMro(lua, classTable);
    for (std::size_t index = 1; index <= classMro.size(); ++index) {
        const lua_glue::Object rawType =
            classMro.raw_get<lua_glue::Object>(index);
        if (!rawType.is<lua_glue::Table>() ||
            !isClass(rawType.as<lua_glue::Table>())) {
            continue;
        }
        for (const auto& entry : rawType.as<lua_glue::Table>()) {
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
        const lua_glue::Object key = lua_glue::MakeObject(lua, property);
        const lua_glue::Object shadow = fields.raw_get<lua_glue::Object>(key);
        const bool hasShadow =
            shadow.valid() && shadow.get_type() != lua_glue::Type::Nil;
        const lua_glue::Object value =
            hasShadow ? shadow : findClassOverride(lua, classTable, key);
        if (!value.valid() || value.get_type() == lua_glue::Type::Nil) {
            continue;
        }
        bool assigned = false;
        for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
            const lua_glue::Object rawType =
                nativeMro.raw_get<lua_glue::Object>(index);
            if (!rawType.is<lua_glue::Table>()) {
                continue;
            }
            if (setNativeObjectMember(lua, nativeObject,
                                      rawType.as<lua_glue::Table>(), key,
                                      value)) {
                assigned = true;
                break;
            }
        }
        if (assigned && hasShadow) {
            shadowSnapshot.emplace_back(key, shadow);
            fields.raw_set(key, lua_glue::nil);
        }
    }
}

void replayNativeDirtyProperties(lua_glue::StateView lua,
                                 const lua_glue::Table& fields,
                                 const lua_glue::Table& root,
                                 const lua_glue::Object& source,
                                 const lua_glue::Object& destination) {
    if ((source.get_type() != lua_glue::Type::Userdata) ||
        (destination.get_type() != lua_glue::Type::Userdata)) {
        return;
    }
    const lua_glue::Object rawDirty =
        fields.raw_get<lua_glue::Object>(NATIVE_DIRTY_PROPERTIES_FIELD);
    if (!rawDirty.is<lua_glue::Table>()) {
        return;
    }
    const lua_glue::Object rawProperties =
        rawDirty.as<lua_glue::Table>().raw_get<lua_glue::Object>(source);
    if (!rawProperties.is<lua_glue::Table>()) {
        return;
    }
    const lua_glue::Table nativeMro = getNativeMro(lua, root);
    for (const auto& entry : rawProperties.as<lua_glue::Table>()) {
        if (!entry.second.is<bool>() || !entry.second.as<bool>()) {
            continue;
        }
        const lua_glue::Object key = entry.first;
        const lua_glue::Object value = protectedIndex(lua, source, key);
        for (std::size_t index = 1; index <= nativeMro.size(); ++index) {
            const lua_glue::Object rawType =
                nativeMro.raw_get<lua_glue::Object>(index);
            if (rawType.is<lua_glue::Table>() &&
                setNativeObjectMember(lua, destination,
                                      rawType.as<lua_glue::Table>(), key,
                                      value)) {
                break;
            }
        }
    }
}

void syncNativeClassDefaults(lua_glue::StateView lua,
                             const lua_glue::Table& classTable,
                             const lua_glue::Object& instance) {
    if (instance.get_type() != lua_glue::Type::Userdata) {
        return;
    }
    const lua_glue::Table fields =
        class_native::getUserFields(lua, instance, false);
    const lua_glue::Object nativeObjects =
        fields.raw_get<lua_glue::Object>(protocol::NATIVE_OBJECTS_FIELD);
    if (!nativeObjects.is<lua_glue::Table>() ||
        tableIsEmpty(nativeObjects.as<lua_glue::Table>())) {
        return;
    }
    std::vector<std::string> properties;
    std::unordered_set<std::string> seenProperties;
    const lua_glue::Table mro = getMro(lua, classTable);
    for (std::size_t index = 2; index <= mro.size(); ++index) {
        const lua_glue::Object rawType = mro.raw_get<lua_glue::Object>(index);
        if (!rawType.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table nativeType = rawType.as<lua_glue::Table>();
        if (!isNativeType(lua, nativeType)) {
            continue;
        }
        const lua_glue::Object rawProperties =
            nativeType.raw_get<lua_glue::Object>(
                protocol::NATIVE_PROPERTIES_FIELD);
        if (!rawProperties.is<lua_glue::Table>()) {
            continue;
        }
        const lua_glue::Table nativeProperties =
            rawProperties.as<lua_glue::Table>();
        for (std::size_t propertyIndex = 1;
             propertyIndex <= nativeProperties.size(); ++propertyIndex) {
            const lua_glue::Object rawName =
                nativeProperties.raw_get<lua_glue::Object>(propertyIndex);
            if (rawName.is<std::string>()) {
                const std::string name = rawName.as<std::string>();
                if (seenProperties.insert(name).second) {
                    properties.push_back(name);
                }
            }
        }
    }
    for (const std::string& property : properties) {
        const lua_glue::Object key = lua_glue::MakeObject(lua, property);
        const lua_glue::Object value = findClassOverride(lua, classTable, key);
        if (value.valid() && value.get_type() != lua_glue::Type::Nil) {
            setNativeMember(lua, fields, classTable, key, value);
        }
    }
}

}  // namespace ludork::standard::class_runtime::detail
