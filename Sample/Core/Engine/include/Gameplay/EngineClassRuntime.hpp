#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <optional>
#include <string>

struct lua_State;

BIND_MODULE_INIT()
LUDORK_ENGINE_API void initializeEngineClassRuntime(lua_State* state);

LUDORK_ENGINE_API void shutdownEngineClassRuntime(lua_State* state) noexcept;

struct EngineResolvedClass {
    RuntimeValue classType;
    RuntimeValue classData;
};

class LUDORK_ENGINE_API EngineClassRuntimeFacade {
public:
    EngineResolvedClass resolve(
        const std::string& classPath,
        const std::optional<std::string>& root = std::nullopt) const;
    RuntimeValue classData(const std::string& classPath) const;
    RuntimeValue instantiateGraph(const std::string& classPath,
                                  const RuntimeValue& parent) const;
    bool graphHasExecutableEvent(const std::string& classPath,
                                 const std::string& eventName) const;
    void invalidate(const std::string& classPath) const;
};

LUDORK_ENGINE_API EngineClassRuntimeFacade& engineClassRuntime();
