#pragma once

#include <RuntimeApi.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

struct lua_State;

namespace ludork::runtime {

enum class ScriptStoreMode : std::uint8_t {
    Loose,
    Packed,
};

class LUDORK_RUNTIME_API ScriptStore final {
public:
    ScriptStore();
    ~ScriptStore();

    ScriptStore(const ScriptStore&) = delete;
    ScriptStore& operator=(const ScriptStore&) = delete;

    void configure(const std::filesystem::path& runtimeRoot);
    void reset() noexcept;

    [[nodiscard]] bool isConfigured() const noexcept;
    [[nodiscard]] ScriptStoreMode mode() const;
    int loadFile(lua_State* state, const std::string& scriptPath) const;
    int loadModule(lua_State* state, const std::string& moduleName) const;
    void registerPreloadedModules(lua_State* state) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] LUDORK_RUNTIME_API ScriptStore& scriptStore();

}  // namespace ludork::runtime
