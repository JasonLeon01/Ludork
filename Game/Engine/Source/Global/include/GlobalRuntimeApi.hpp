#pragma once

#include <cstdint>
#include <optional>

#if defined(_WIN32)
#if defined(LUDORK_GLOBAL_EXPORTS)
#define LUDORK_GLOBAL_API __declspec(dllexport)
#else
#define LUDORK_GLOBAL_API __declspec(dllimport)
#endif
#else
#define LUDORK_GLOBAL_API __attribute__((visibility("default")))
#endif

struct lua_State;

LUDORK_GLOBAL_API void initializeGlobalLifecycle(lua_State* state);

namespace ludork::global {

enum class RuntimeWindowMode {
    PlatformDefault,
    Embedded,
    Individual,
};

struct RuntimeLaunchOptions {
    bool editor = false;
    RuntimeWindowMode windowMode = RuntimeWindowMode::PlatformDefault;
    std::optional<std::uintptr_t> hostWindowHandle;
};

LUDORK_GLOBAL_API void setRuntimeLaunchOptions(
    const RuntimeLaunchOptions& options) noexcept;

LUDORK_GLOBAL_API const RuntimeLaunchOptions& runtimeLaunchOptions() noexcept;

LUDORK_GLOBAL_API void shutdown(lua_State* state) noexcept;

}  // namespace ludork::global
