#pragma once

#include <Runtime/RuntimeValue.hpp>

enum class ComponentRuntimeCacheKind {
    Types,
    FieldDefaults,
    FieldMap,
    InheritedDefaults,
};

class ComponentRuntimeCache {
public:
    void clear() const;
    RuntimeValue get(ComponentRuntimeCacheKind kind,
                     const RuntimeValue& key) const;
    void set(ComponentRuntimeCacheKind kind, const RuntimeValue& key,
             const RuntimeValue& value) const;
};

ComponentRuntimeCache& componentRuntimeCache();
