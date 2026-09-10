#pragma once

#include <Runtime/RuntimeValue.hpp>

struct lua_State;

class ComponentRuntimeCache {
public:
    enum class ComponentRuntimeCacheKind {
        Types,
        FieldDefaults,
        FieldMap,
        InheritedDefaults,
    };

    void clear(lua_State* state) const noexcept;
    RuntimeValue get(ComponentRuntimeCacheKind kind,
                     const RuntimeValue& key) const;
    void set(ComponentRuntimeCacheKind kind, const RuntimeValue& key,
             const RuntimeValue& value) const;
};

ComponentRuntimeCache& componentRuntimeCache();
