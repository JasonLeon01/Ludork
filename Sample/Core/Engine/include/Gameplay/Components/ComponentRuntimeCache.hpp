#pragma once

#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

enum class ComponentRuntimeCacheKind {
    Types,
    FieldDefaults,
    FieldMap,
    InheritedDefaults,
};

class LUDORK_ENGINE_API ComponentRuntimeCache {
public:
    RuntimeValue get(ComponentRuntimeCacheKind kind,
                     const RuntimeValue& key) const;
    void set(ComponentRuntimeCacheKind kind, const RuntimeValue& key,
             const RuntimeValue& value) const;
};

LUDORK_ENGINE_API ComponentRuntimeCache& componentRuntimeCache();
