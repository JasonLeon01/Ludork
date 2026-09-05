#pragma once

#include <Runtime/RuntimeValue.hpp>

struct lua_State;

enum class ComponentRuntimeCacheKind {
    Types,
    FieldDefaults,
    FieldMap,
    InheritedDefaults,
};

class ComponentRuntimeCache {
public:
    void clear(lua_State* state) const noexcept;
    RuntimeValue get(ComponentRuntimeCacheKind kind,
                     const RuntimeValue& key) const;
    void set(ComponentRuntimeCacheKind kind, const RuntimeValue& key,
             const RuntimeValue& value) const;
};

ComponentRuntimeCache& componentRuntimeCache();
