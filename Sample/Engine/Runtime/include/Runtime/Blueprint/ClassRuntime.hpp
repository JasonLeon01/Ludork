#pragma once

#include <CoreMinimal.hpp>

#include <RuntimeApi.hpp>

struct ResolvedClass {
    RuntimeHandle classType;
    RuntimeValue classData;
};

class LUDORK_RUNTIME_API ClassRuntimeFacade {
public:
    ResolvedClass resolve(
        const std::string& classPath,
        const std::optional<std::string>& root = std::nullopt) const;
    RuntimeValue classData(const std::string& classPath) const;
    RuntimeValue instantiateGraph(const std::string& classPath,
                                  const RuntimeValue& parent) const;
    bool graphHasExecutableEvent(const std::string& classPath,
                                 const std::string& eventName) const;
    bool containsCached(const std::string& classPath) const;
    std::optional<std::string> findCachedPathByName(
        const std::string& className) const;
};

LUDORK_RUNTIME_API ClassRuntimeFacade& classRuntime();
