#pragma once

#include <CoreMinimal.hpp>
#include <RuntimeApi.hpp>

BIND_CLASS(metadata = false)
class LUDORK_RUNTIME_API ClassDict {
public:
    BIND_INIT()
    ClassDict() = default;

    BIND_METHOD(indexer = true, metadata = false)
    RuntimeValue get(const std::optional<std::string>& classPath,
                     const std::optional<std::string>& root = std::nullopt);

    /// @brief Resolve and return the session's shared JSON definition.
    /// @return The definition table, or nil for a module class. Unknown paths
    /// raise. Changes do not invalidate generated classes, instance defaults or
    /// graph templates. Use copy/deepcopy when an independent table is needed.
    BIND_METHOD(metadata = false)
    RuntimeValue getData(const std::string& classPath);

    BIND_METHOD(metadata = false, allow_nil = "parent")
    RuntimeValue instantiateGraph(const std::string& classPath,
                                  RuntimeValue parent);

    BIND_METHOD(metadata = false)
    bool containsCached(const std::string& classPath) const;

    BIND_METHOD(metadata = false)
    std::optional<std::string> findCachedPathByName(
        const std::string& className) const;
};
