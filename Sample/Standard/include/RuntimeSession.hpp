#pragma once

#include <StandardApi.hpp>

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct lua_State;

namespace ludork::standard {

class LuaRegistryReference;
struct RuntimeRegistryReferenceState;
using RuntimeCleanup = void (*)(lua_State*) noexcept;

enum class RuntimeSessionPhase {
    running,
    stopping,
    stopped,
};

struct RuntimeSessionState {
    explicit RuntimeSessionState(lua_State* value) : state(value) {}

    lua_State* state;
    std::recursive_mutex luaMutex;
    std::atomic<RuntimeSessionPhase> phase{RuntimeSessionPhase::running};
    std::thread::id shutdownThread;
    std::unordered_set<RuntimeRegistryReferenceState*> registryReferences;
    std::unordered_map<const void*, const LuaRegistryReference*> opaqueValues;
    std::vector<RuntimeCleanup> moduleCleanups;
};

class LUDORK_STANDARD_API LuaExecutionScope {
public:
    explicit LuaExecutionScope(lua_State* state);
    ~LuaExecutionScope();

    LuaExecutionScope(const LuaExecutionScope&) = delete;
    LuaExecutionScope& operator=(const LuaExecutionScope&) = delete;

    bool active() const noexcept;

private:
    lua_State* state_ = nullptr;
    bool active_ = false;
};

class LUDORK_STANDARD_API LuaExecutionPause {
public:
    LuaExecutionPause() noexcept;
    ~LuaExecutionPause();

    LuaExecutionPause(const LuaExecutionPause&) = delete;
    LuaExecutionPause& operator=(const LuaExecutionPause&) = delete;

private:
    lua_State* state_ = nullptr;
    std::shared_ptr<RuntimeSessionState> session_;
    std::size_t depth_ = 0;
};

class LUDORK_STANDARD_API LuaRegistryReference {
public:
    LuaRegistryReference() noexcept = default;
    LuaRegistryReference(lua_State* state, int stackIndex);

    lua_State* state() const noexcept;
    bool push() const noexcept;
    bool pushUnderExecutionScope() const noexcept;
    bool equals(const LuaRegistryReference& other) const noexcept;
    explicit operator bool() const noexcept;

private:
    std::shared_ptr<RuntimeRegistryReferenceState> reference_;
};

LUDORK_STANDARD_API void registerRuntimeOpaqueValue(
    const void* identity, const LuaRegistryReference& reference);
LUDORK_STANDARD_API void unregisterRuntimeOpaqueValue(
    lua_State* state, const void* identity) noexcept;
LUDORK_STANDARD_API LuaRegistryReference
findRuntimeOpaqueValue(lua_State* state, const void* identity);
LUDORK_STANDARD_API void initializeRuntimeSession(lua_State* state);
LUDORK_STANDARD_API int enterRuntimeSession(lua_State* state) noexcept;
LUDORK_STANDARD_API int tryEnterRuntimeSession(lua_State* state) noexcept;
LUDORK_STANDARD_API void leaveRuntimeSession(lua_State* state) noexcept;
LUDORK_STANDARD_API void beginRuntimeShutdown(lua_State* state) noexcept;
LUDORK_STANDARD_API void registerRuntimeCleanup(lua_State* state,
                                                RuntimeCleanup cleanup);
LUDORK_STANDARD_API void runRuntimeCleanups(lua_State* state) noexcept;
LUDORK_STANDARD_API void clearRuntimeRegistryReferences(
    lua_State* state) noexcept;
LUDORK_STANDARD_API void releaseRuntimeSession(lua_State* state) noexcept;
LUDORK_STANDARD_API bool isRuntimeStopping(lua_State* state) noexcept;

}  // namespace ludork::standard
