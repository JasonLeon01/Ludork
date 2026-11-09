#include <LuaError.hpp>
#include <Runtime/ScriptStore.hpp>
#include <LudorkGenerated/ResourceFileConstants.hpp>

#include "ScriptStoreImpl.hpp"
#include "ScriptModuleShape.hpp"
#include "LdPakArchive.hpp"
#include "ResourceStorePaths.hpp"
#include <Utf8Path.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::runtime {
namespace {

constexpr std::string_view LuaDeclarationSuffix = ".d.lua";
constexpr const char* PRELOAD_OWNERS = "Ludork.ScriptStore.preloadOwners";

void pushPreloadOwners(lua_State* state, const ScriptStore* store,
                       bool create) {
    if (create) {
        luaL_getsubtable(state, LUA_REGISTRYINDEX, PRELOAD_OWNERS);
    } else {
        lua_getfield(state, LUA_REGISTRYINDEX, PRELOAD_OWNERS);
        if (!lua_istable(state, -1)) {
            lua_pop(state, 1);
            lua_pushnil(state);
            return;
        }
    }
    lua_rawgetp(state, -1, store);
    if (!lua_istable(state, -1) && create) {
        lua_pop(state, 1);
        lua_newtable(state);
        lua_pushvalue(state, -1);
        lua_rawsetp(state, -3, store);
    }
    lua_remove(state, -2);
}

bool isDeclarationPath(const std::string_view path) {
    const std::string folded =
        ludork::runtime::detail::asciiFold(std::string(path));
    return folded == "stub" || folded.starts_with("stub/") ||
           folded.ends_with(LuaDeclarationSuffix);
}

bool isIgnoredMetadata(const std::filesystem::path& path,
                       const std::filesystem::file_status& status) {
    return std::filesystem::is_regular_file(status) &&
           path.filename() == ".DS_Store";
}

bool isLuaScriptPath(const std::string_view path) {
    return path.ends_with(ludork::generated::resources::LuaSourceExtension) ||
           path.ends_with(ludork::generated::resources::LuaCompiledExtension);
}

std::string moduleName(const std::string& path) {
    std::size_t extensionSize = 0;
    if (path.ends_with(ludork::generated::resources::LuaCompiledExtension)) {
        extensionSize =
            std::string_view(ludork::generated::resources::LuaCompiledExtension)
                .size();
    } else if (path.ends_with(
                   ludork::generated::resources::LuaSourceExtension)) {
        extensionSize =
            std::string_view(ludork::generated::resources::LuaSourceExtension)
                .size();
    } else {
        return {};
    }
    const std::string_view stem(path.data(), path.size() - extensionSize);
    std::string result;
    std::size_t start = 0;
    while (start < stem.size()) {
        const std::size_t separator = stem.find('/', start);
        const std::size_t end =
            separator == std::string_view::npos ? stem.size() : separator;
        const std::string_view segment = stem.substr(start, end - start);
        if (segment.empty() || segment.find('.') != std::string_view::npos) {
            return {};
        }
        if (!result.empty()) {
            result.push_back('.');
        }
        result.append(segment);
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1;
    }
    return result;
}

std::string validateScriptPath(const std::string& scriptPath) {
    constexpr std::string_view Prefix =
        ludork::generated::resources::ScriptPathPrefix;
    const std::string path = scriptPath.starts_with("/Game/Scripts/")
                                 ? scriptPath.substr(6)
                                 : scriptPath;
    if (!path.starts_with(Prefix) || path.size() == Prefix.size() ||
        path.find('\\') != std::string::npos ||
        path.find('\0') != std::string::npos ||
        (!path.ends_with(ludork::generated::resources::LuaSourceExtension) &&
         !path.ends_with(ludork::generated::resources::LuaCompiledExtension))) {
        throw std::invalid_argument(
            "Script path must name a .lua or .luac file under Scripts");
    }
    const std::string relative = path.substr(Prefix.size());
    std::size_t start = 0;
    while (start < relative.size()) {
        const std::size_t separator = relative.find('/', start);
        const std::size_t end =
            separator == std::string::npos ? relative.size() : separator;
        const std::string_view segment(relative.data() + start, end - start);
        if (segment.empty() || segment == "." || segment == ".." ||
            segment.find(':') != std::string_view::npos) {
            throw std::invalid_argument("Script path is not canonical: " +
                                        scriptPath);
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    if (isDeclarationPath(relative)) {
        throw std::invalid_argument(
            "Lua declaration files cannot be loaded at runtime");
    }
    return relative;
}

std::vector<std::uint8_t> readPhysicalFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open Script file: " +
                                 ludork::standard::pathToUtf8(path));
    }
    std::vector<std::uint8_t> result{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>(),
    };
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("Failed to read Script file: " +
                                 ludork::standard::pathToUtf8(path));
    }
    return result;
}

void addScriptEntry(
    std::unordered_map<std::string, script_store_impl::ScriptEntry>& entries,
    std::unordered_map<std::string, std::string>& foldedPaths,
    const std::string& relative, script_store_impl::ScriptEntry entry) {
    if (!entries.emplace(relative, std::move(entry)).second) {
        throw std::runtime_error("Duplicate Script path: " + relative);
    }
    const std::string folded = ludork::runtime::detail::asciiFold(relative);
    const auto [iterator, inserted] = foldedPaths.emplace(folded, relative);
    if (!inserted && iterator->second != relative) {
        throw std::runtime_error("Script paths differ only by case: " +
                                 iterator->second + " and " + relative);
    }
}

void addModule(std::unordered_map<std::string, std::string>& modules,
               const std::string& relative) {
    const std::string name = moduleName(relative);
    if (name.empty()) {
        return;
    }
    const auto existing = modules.find(name);
    if (existing == modules.end()) {
        modules.emplace(name, relative);
        return;
    }
    const bool newSource =
        relative.ends_with(ludork::generated::resources::LuaSourceExtension);
    const bool oldSource = existing->second.ends_with(
        ludork::generated::resources::LuaSourceExtension);
    if (newSource == oldSource) {
        throw std::runtime_error("Multiple Script files map to module " + name);
    }
    if (newSource) {
        existing->second = relative;
    }
}

std::string alternateScriptPath(const std::string& relative) {
    if (relative.ends_with(ludork::generated::resources::LuaSourceExtension)) {
        return relative.substr(
                   0, relative.size() -
                          std::string_view(
                              ludork::generated::resources::LuaSourceExtension)
                              .size()) +
               ludork::generated::resources::LuaCompiledExtension;
    }
    if (relative.ends_with(
            ludork::generated::resources::LuaCompiledExtension)) {
        return relative.substr(
                   0,
                   relative.size() -
                       std::string_view(
                           ludork::generated::resources::LuaCompiledExtension)
                           .size()) +
               ludork::generated::resources::LuaSourceExtension;
    }
    return {};
}

std::optional<std::string> modulePath(const std::string& name) {
    if (name.empty() || name.front() == '.' || name.back() == '.' ||
        name.find("..") != std::string::npos ||
        name.find_first_of("/\\:") != std::string::npos ||
        name.find('\0') != std::string::npos) {
        return std::nullopt;
    }
    std::string path = name;
    std::replace(path.begin(), path.end(), '.', '/');
    path += ludork::generated::resources::LuaSourceExtension;
    if (isDeclarationPath(path)) {
        return std::nullopt;
    }
    return path;
}

int loadScriptEntry(lua_State* state, const std::string& relative,
                    const script_store_impl::ScriptEntry& entry,
                    const char* mode) {
    const std::vector<std::uint8_t> source =
        entry.archive ? entry.archive->readAll(entry.archivePath)
                      : readPhysicalFile(entry.source);
    const std::string chunkName = "@Scripts/" + relative;
    const char* sourceData =
        source.empty() ? "" : reinterpret_cast<const char*>(source.data());
    return luaL_loadbufferx(state, sourceData, source.size(), chunkName.c_str(),
                            mode);
}

bool captureDefinitions(const ScriptStore& store) {
    const char* editor = std::getenv("LUDORK_EDITOR");
    return editor != nullptr && std::string_view(editor) == "1" &&
           store.mode() == ScriptStoreMode::Loose;
}

int preloadScript(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        ScriptStore* store = static_cast<ScriptStore*>(
            lua_touserdata(state, lua_upvalueindex(1)));
        std::size_t moduleLength = 0;
        const char* moduleValue =
            lua_tolstring(state, lua_upvalueindex(2), &moduleLength);
        if (store == nullptr || moduleValue == nullptr) {
            throw std::runtime_error("Invalid Script preload closure");
        }
        int status =
            store->loadModule(state, std::string(moduleValue, moduleLength));
        if (status != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state, -1));
        }
        const int argumentCount = lua_gettop(state) - 1;
        lua_insert(state, 1);
        status = lua_pcall(state, argumentCount, LUA_MULTRET, 0);
        if (status != LUA_OK) {
            return lua_error(state);
        }
        if (lua_toboolean(state, lua_upvalueindex(3)) &&
            lua_gettop(state) > 0) {
            captureScriptModuleShape(state,
                                     std::string(moduleValue, moduleLength), 1);
        }
        return lua_gettop(state);
    });
}

void pushModuleLoader(lua_State* state, const ScriptStore* store,
                      const std::string& name, bool capture) {
    lua_pushlightuserdata(state, const_cast<ScriptStore*>(store));
    lua_pushlstring(state, name.data(), name.size());
    lua_pushboolean(
        state,
        capture && !name.ends_with(
                       ludork::generated::resources::MetadataModuleSuffix));
    lua_pushcclosure(state, preloadScript, 3);
}

int searchScript(lua_State* state) {
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        ScriptStore* store = static_cast<ScriptStore*>(
            lua_touserdata(state, lua_upvalueindex(1)));
        std::size_t length = 0;
        const char* value = luaL_checklstring(state, 1, &length);
        const std::string name(value, length);
        if (!store->hasModule(name)) {
            lua_pushfstring(state, "\n\tno Ludork script module '%s'", value);
            return 1;
        }
        pushModuleLoader(state, store, name, captureDefinitions(*store));
        luaL_getsubtable(state, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
        lua_pushvalue(state, -2);
        lua_setfield(state, -2, name.c_str());
        lua_pop(state, 1);
        pushPreloadOwners(state, store, true);
        lua_pushvalue(state, -2);
        lua_setfield(state, -2, name.c_str());
        lua_pop(state, 1);
        lua_pushliteral(state, "Ludork.ScriptStore");
        return 2;
    });
}

bool isVirtualScript(lua_State* state) {
    std::size_t length = 0;
    const char* value = lua_tolstring(state, 1, &length);
    return value != nullptr &&
           std::string_view(value, length).starts_with("/Game/Scripts/");
}

int finishFileCall(lua_State* state, int, lua_KContext base) {
    return lua_gettop(state) - static_cast<int>(base);
}

int loadScriptFile(lua_State* state) {
    if (!isVirtualScript(state)) {
        const int arguments = lua_gettop(state);
        lua_pushvalue(state, lua_upvalueindex(2));
        lua_insert(state, 1);
        lua_callk(state, arguments, LUA_MULTRET, 0, finishFileCall);
        return lua_gettop(state);
    }
    return ludork::standard::protectedLuaCallback(state, [&]() -> int {
        ScriptStore* store = static_cast<ScriptStore*>(
            lua_touserdata(state, lua_upvalueindex(1)));
        std::size_t length = 0;
        const char* filename = luaL_checklstring(state, 1, &length);
        const char* mode = luaL_optstring(state, 2, nullptr);
        const bool environment = !lua_isnone(state, 3);
        const int status =
            store->loadFile(state, std::string(filename, length), mode);
        if (status != LUA_OK) {
            lua_pushnil(state);
            lua_insert(state, -2);
            return 2;
        }
        if (environment) {
            lua_pushvalue(state, 3);
            if (lua_setupvalue(state, -2, 1) == nullptr) {
                lua_pop(state, 1);
            }
        }
        return 1;
    });
}

int executeScriptFile(lua_State* state) {
    if (!isVirtualScript(state)) {
        const int arguments = lua_gettop(state);
        lua_pushvalue(state, lua_upvalueindex(2));
        lua_insert(state, 1);
        lua_callk(state, arguments, LUA_MULTRET, 0, finishFileCall);
        return lua_gettop(state);
    }
    lua_settop(state, 1);
    const int status =
        ludork::standard::protectedLuaCallback(state, [&]() -> int {
            ScriptStore* store = static_cast<ScriptStore*>(
                lua_touserdata(state, lua_upvalueindex(1)));
            std::size_t length = 0;
            const char* filename = lua_tolstring(state, 1, &length);
            return store->loadFile(state, std::string(filename, length));
        });
    if (status != LUA_OK) {
        return lua_error(state);
    }
    lua_callk(state, 0, LUA_MULTRET, 1, finishFileCall);
    return lua_gettop(state) - 1;
}

void installScriptLoaders(lua_State* state, const ScriptStore* store) {
    luaL_getsubtable(state, LUA_REGISTRYINDEX, "Ludork.ScriptStore.loaders");
    lua_rawgetp(state, -1, store);
    const bool installed = lua_toboolean(state, -1);
    lua_pop(state, 1);
    if (installed) {
        lua_pop(state, 1);
        return;
    }
    lua_pushboolean(state, true);
    lua_rawsetp(state, -2, store);
    lua_pop(state, 1);
    lua_getglobal(state, "package");
    lua_getfield(state, -1, "searchers");
    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(state, -1));
    for (lua_Integer index = count; index >= 2; --index) {
        lua_rawgeti(state, -1, index);
        lua_rawseti(state, -2, index + 1);
    }
    lua_pushlightuserdata(state, const_cast<ScriptStore*>(store));
    lua_pushcclosure(state, searchScript, 1);
    lua_rawseti(state, -2, 2);
    lua_pop(state, 2);
    lua_pushlightuserdata(state, const_cast<ScriptStore*>(store));
    lua_getglobal(state, "loadfile");
    lua_pushcclosure(state, loadScriptFile, 2);
    lua_setglobal(state, "loadfile");
    lua_pushlightuserdata(state, const_cast<ScriptStore*>(store));
    lua_getglobal(state, "dofile");
    lua_pushcclosure(state, executeScriptFile, 2);
    lua_setglobal(state, "dofile");
}

}  // namespace

ScriptStore::ScriptStore() : impl_(std::make_unique<Impl>()) {}
ScriptStore::~ScriptStore() = default;

void ScriptStore::configure(const std::filesystem::path& runtimeRoot) {
    std::error_code error;
    const std::filesystem::path normalized =
        std::filesystem::weakly_canonical(runtimeRoot, error);
    if (error || normalized.empty()) {
        throw std::invalid_argument("Invalid runtime root for ScriptStore");
    }
    const std::filesystem::path scriptsRoot =
        normalized / ludork::generated::resources::ScriptGroup;
    const std::filesystem::path packagePath =
        normalized / (std::string(ludork::generated::resources::ScriptGroup) +
                      ludork::generated::resources::PackageExtension);
    error.clear();
    const bool looseExists = std::filesystem::exists(scriptsRoot, error);
    if (error) {
        throw std::runtime_error("Failed to inspect Scripts: " +
                                 error.message());
    }
    error.clear();
    const bool packageExists = std::filesystem::exists(packagePath, error);
    if (error) {
        throw std::runtime_error("Failed to inspect Scripts.ldpak: " +
                                 error.message());
    }
    if (looseExists == packageExists) {
        throw std::runtime_error(
            "Runtime root must contain exactly one of Scripts or " +
            (std::string(ludork::generated::resources::ScriptGroup) +
             ludork::generated::resources::PackageExtension));
    }

    std::unordered_map<std::string, script_store_impl::ScriptEntry>
        loadedEntries;
    std::unordered_map<std::string, std::string> foldedPaths;
    std::unordered_map<std::string, std::string> loadedModules;
    std::shared_ptr<detail::LdPakArchive> loadedArchive;
    const ScriptStoreMode loadedMode =
        looseExists ? ScriptStoreMode::Loose : ScriptStoreMode::Packed;
    if (loadedMode == ScriptStoreMode::Loose) {
        const std::filesystem::file_status scriptsStatus =
            std::filesystem::symlink_status(scriptsRoot, error);
        if (error || !std::filesystem::is_directory(scriptsStatus) ||
            ludork::runtime::detail::isLinkLike(
                scriptsRoot, scriptsStatus,
                "Failed to inspect Scripts filesystem entry")) {
            throw std::runtime_error("Scripts must be a real directory");
        }
        std::filesystem::recursive_directory_iterator iterator(
            scriptsRoot, std::filesystem::directory_options::none, error);
        if (error) {
            throw std::runtime_error("Failed to enumerate Scripts: " +
                                     error.message());
        }
        const std::filesystem::recursive_directory_iterator end;
        while (iterator != end) {
            const std::filesystem::directory_entry entry = *iterator;
            const std::filesystem::file_status status =
                entry.symlink_status(error);
            if (error) {
                throw std::runtime_error("Failed to inspect Script entry: " +
                                         error.message());
            }
            if (ludork::runtime::detail::isLinkLike(
                    entry.path(), status,
                    "Failed to inspect Scripts filesystem entry")) {
                throw std::runtime_error(
                    "Script symlinks are not supported: " +
                    ludork::standard::pathToUtf8(entry.path()));
            }
            const std::string relative = ludork::standard::pathToGenericUtf8(
                entry.path().lexically_relative(scriptsRoot));
            if (isDeclarationPath(relative)) {
                if (std::filesystem::is_directory(status)) {
                    iterator.disable_recursion_pending();
                }
            } else if (isIgnoredMetadata(entry.path(), status) ||
                       std::filesystem::is_directory(status)) {
            } else if (std::filesystem::is_regular_file(status) &&
                       isLuaScriptPath(relative)) {
                addScriptEntry(loadedEntries, foldedPaths, relative,
                               {entry.path(), {}});
                addModule(loadedModules, relative);
            } else if (!std::filesystem::is_regular_file(status)) {
                throw std::runtime_error(
                    "Unsupported Script entry: " +
                    ludork::standard::pathToUtf8(entry.path()));
            }
            iterator.increment(error);
            if (error) {
                throw std::runtime_error("Failed to enumerate Scripts: " +
                                         error.message());
            }
        }
    } else {
        loadedArchive = std::make_shared<detail::LdPakArchive>(packagePath);
        if (loadedArchive->group() !=
            ludork::generated::resources::ScriptGroup) {
            throw std::runtime_error(
                "Scripts.ldpak must use the Scripts group");
        }
        for (const detail::LdPakEntry& entry : loadedArchive->entries()) {
            if (isDeclarationPath(entry.path)) {
                throw std::runtime_error(
                    "Scripts.ldpak must not contain Lua declarations: " +
                    entry.path);
            }
            if (entry.directory) {
                continue;
            }
            if (entry.path.ends_with("/.DS_Store") ||
                entry.path == ".DS_Store") {
                continue;
            }
            if (!isLuaScriptPath(entry.path)) {
                continue;
            }
            addScriptEntry(loadedEntries, foldedPaths, entry.path,
                           {loadedArchive->path(), entry.path, loadedArchive});
            addModule(loadedModules, entry.path);
        }
    }
    if (!loadedEntries.contains(
            ludork::generated::resources::ScriptEntrySource) &&
        !loadedEntries.contains(
            ludork::generated::resources::ScriptEntryCompiled)) {
        throw std::runtime_error(
            "Scripts must contain Entry.lua or Entry.luac");
    }

    std::vector<std::string> orderedModules;
    orderedModules.reserve(loadedModules.size());
    for (const auto& [name, relative] : loadedModules) {
        static_cast<void>(relative);
        orderedModules.push_back(name);
    }
    std::sort(orderedModules.begin(), orderedModules.end());

    std::unique_lock lock(impl_->mutex);
    impl_->runtimeRoot = normalized;
    impl_->userDataRoot.clear();
    impl_->mode = loadedMode;
    impl_->archive = std::move(loadedArchive);
    impl_->entries = std::move(loadedEntries);
    impl_->modules = std::move(loadedModules);
    impl_->orderedModules = std::move(orderedModules);
    impl_->configured = true;
    ++impl_->generation;
    impl_->reloadOwner = nullptr;
}

void ScriptStore::setUserDataRoot(const std::filesystem::path& root) {
    const std::filesystem::path normalized = detail::resourceUserDataRoot(root);
    std::unique_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("ScriptStore is not configured");
    }
    if (impl_->userDataRoot != normalized) {
        impl_->userDataRoot = normalized;
        ++impl_->generation;
    }
}

std::optional<std::pair<std::string, script_store_impl::ScriptEntry>>
ScriptStore::Impl::resolve(const std::string& relative) const {
    if (!configured) {
        throw std::logic_error("ScriptStore is not configured");
    }
    for (const bool user : {true, false}) {
        for (const std::string& candidate :
             {relative, alternateScriptPath(relative)}) {
            if (user || mode == ScriptStoreMode::Loose) {
                const auto entry = detail::findLooseResource(
                    user ? userDataRoot : runtimeRoot,
                    std::filesystem::path(
                        ludork::generated::resources::ScriptGroup) /
                        ludork::standard::pathFromUtf8(candidate));
                if (entry) {
                    if (!entry->is_regular_file()) {
                        throw std::runtime_error("Script is not a file: " +
                                                 candidate);
                    }
                    return std::make_pair(
                        candidate,
                        script_store_impl::ScriptEntry{entry->path(), {}, {}});
                }
            } else {
                const auto entry = entries.find(candidate);
                if (entry != entries.end()) {
                    return std::make_pair(candidate, entry->second);
                }
            }
        }
    }
    return std::nullopt;
}

void ScriptStore::reset() noexcept {
    std::unique_lock lock(impl_->mutex);
    impl_->runtimeRoot.clear();
    impl_->userDataRoot.clear();
    impl_->archive.reset();
    impl_->entries.clear();
    impl_->modules.clear();
    impl_->orderedModules.clear();
    impl_->mode = ScriptStoreMode::Loose;
    impl_->configured = false;
    ++impl_->generation;
    impl_->reloadOwner = nullptr;
}

bool ScriptStore::isConfigured() const noexcept {
    std::shared_lock lock(impl_->mutex);
    return impl_->configured;
}

ScriptStoreMode ScriptStore::mode() const {
    std::shared_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("ScriptStore is not configured");
    }
    return impl_->mode;
}

int ScriptStore::loadFile(lua_State* state, const std::string& scriptPath,
                          const char* mode) const {
    if (state == nullptr) {
        throw std::invalid_argument("Lua state must not be null");
    }
    try {
        const std::string relative = validateScriptPath(scriptPath);
        std::shared_lock lock(impl_->mutex);
        if (!impl_->configured) {
            throw std::logic_error("ScriptStore is not configured");
        }
        const auto entry = impl_->resolve(relative);
        if (!entry) {
            throw std::runtime_error("Script file not found: Scripts/" +
                                     relative);
        }
        return loadScriptEntry(state, entry->first, entry->second, mode);
    } catch (const std::exception& exception) {
        lua_pushstring(state, exception.what());
        return LUA_ERRFILE;
    }
}

bool ScriptStore::hasModule(const std::string& name) const {
    const auto relative = modulePath(name);
    if (!relative) {
        return false;
    }
    std::shared_lock lock(impl_->mutex);
    return impl_->resolve(*relative).has_value();
}

int ScriptStore::loadModule(lua_State* state, const std::string& name) const {
    if (state == nullptr) {
        throw std::invalid_argument("Lua state must not be null");
    }
    try {
        const auto relative = modulePath(name);
        if (!relative) {
            throw std::invalid_argument("Invalid Script module name: " + name);
        }
        std::shared_lock lock(impl_->mutex);
        const auto entry = impl_->resolve(*relative);
        if (!entry) {
            throw std::runtime_error("Script module not found: " + name);
        }
        return loadScriptEntry(state, entry->first, entry->second, nullptr);
    } catch (const std::exception& exception) {
        lua_pushstring(state, exception.what());
        return LUA_ERRFILE;
    }
}

void ScriptStore::registerPreloadedModules(lua_State* state) const {
    if (state == nullptr) {
        throw std::invalid_argument("Lua state must not be null");
    }
    std::shared_lock lock(impl_->mutex);
    if (!impl_->configured) {
        throw std::logic_error("ScriptStore is not configured");
    }
    luaL_getsubtable(state, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
    const int loadedIndex = lua_gettop(state);
    luaL_getsubtable(state, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    const int preloadIndex = lua_gettop(state);
    pushPreloadOwners(state, this, true);
    const int ownersIndex = lua_gettop(state);
    const char* editor = std::getenv("LUDORK_EDITOR");
    const bool captureDefinitions = editor != nullptr &&
                                    std::string_view(editor) == "1" &&
                                    impl_->mode == ScriptStoreMode::Loose;
    for (const std::string& name : impl_->orderedModules) {
        lua_getfield(state, loadedIndex, name.c_str());
        const bool loaded = !lua_isnil(state, -1);
        lua_pop(state, 1);
        lua_getfield(state, preloadIndex, name.c_str());
        const bool preloaded = !lua_isnil(state, -1);
        lua_pop(state, 1);

        if (loaded || preloaded) {
            continue;
        }
        pushModuleLoader(state, this, name, captureDefinitions);
        lua_pushvalue(state, -1);
        lua_setfield(state, ownersIndex, name.c_str());
        lua_setfield(state, preloadIndex, name.c_str());
    }
    lua_pop(state, 3);
    installScriptLoaders(state, this);
}

ScriptStore::ReloadSnapshot ScriptStore::prepareReload() const {
    ReloadSnapshot snapshot;
    std::filesystem::path root;
    std::filesystem::path userRoot;
    std::uint64_t generation = 0;
    {
        std::shared_lock lock(impl_->mutex);
        if (!impl_->configured || impl_->mode != ScriptStoreMode::Loose) {
            throw std::runtime_error(
                "Hot reload requires loose Lua source files");
        }
        root = impl_->runtimeRoot;
        userRoot = impl_->userDataRoot;
        generation = impl_->generation;
    }
    snapshot.candidate = std::make_unique<ScriptStore>();
    snapshot.candidate->configure(root);
    snapshot.candidate->setUserDataRoot(userRoot);
    Impl& candidate = *snapshot.candidate->impl_;
    if (candidate.mode != ScriptStoreMode::Loose) {
        throw std::runtime_error(
            "Hot reload cannot switch script storage mode");
    }
    candidate.reloadOwner = this;
    candidate.reloadGeneration = generation;
    std::set<std::string> paths;
    for (const auto& [path, entry] : candidate.entries) {
        static_cast<void>(entry);
        paths.insert(path);
    }
    const auto userScripts = detail::findLooseResource(
        userRoot,
        std::filesystem::path(ludork::generated::resources::ScriptGroup));
    if (userScripts) {
        if (!userScripts->is_directory()) {
            throw std::runtime_error("User Scripts must be a directory");
        }
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 userScripts->path())) {
            const std::string path = ludork::standard::pathToGenericUtf8(
                entry.path().lexically_relative(userScripts->path()));
            if (entry.is_regular_file() && isLuaScriptPath(path) &&
                !isDeclarationPath(path)) {
                paths.insert(path);
            }
        }
    }
    for (const std::string& requested : paths) {
        if (requested.ends_with(
                ludork::generated::resources::MetadataSourceSuffix) ||
            requested.ends_with(
                ludork::generated::resources::MetadataCompiledSuffix)) {
            continue;
        }
        std::string preferred = requested;
        if (preferred.ends_with(
                ludork::generated::resources::LuaCompiledExtension)) {
            preferred = alternateScriptPath(preferred);
        }
        const auto selected = candidate.resolve(preferred);
        if (!selected) {
            throw std::runtime_error("Script disappeared during hot reload: " +
                                     requested);
        }
        const auto& [path, entry] = *selected;
        if (!path.ends_with(ludork::generated::resources::LuaSourceExtension)) {
            throw std::runtime_error(
                "Hot reload does not support bytecode: Scripts/" + path);
        }
        const std::string key =
            ludork::generated::resources::ScriptPathPrefix + path;
        if (snapshot.sources.contains(key)) {
            continue;
        }
        const std::vector<std::uint8_t> bytes = readPhysicalFile(entry.source);
        snapshot.sources.emplace(key, std::string(bytes.begin(), bytes.end()));
        const std::string name = moduleName(path);
        if (!name.empty()) {
            snapshot.modules.emplace(name, key);
        }
    }
    return snapshot;
}

bool ScriptStore::ownsModule(lua_State* state, const std::string& name) const {
    if (state == nullptr) {
        throw std::invalid_argument("Lua state must not be null");
    }
    pushPreloadOwners(state, this, false);
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        return false;
    }
    lua_getfield(state, -1, name.c_str());
    lua_getfield(state, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    bool owned = false;
    if (lua_istable(state, -1)) {
        lua_getfield(state, -1, name.c_str());
        owned = !lua_isnil(state, -3) && lua_rawequal(state, -1, -3);
        lua_pop(state, 1);
    }
    lua_pop(state, 3);
    return owned;
}

void ScriptStore::publishReload(lua_State* state, ReloadSnapshot& snapshot) {
    if (state == nullptr) {
        throw std::invalid_argument("Lua state must not be null");
    }
    if (snapshot.candidate == nullptr || snapshot.candidate.get() == this) {
        throw std::invalid_argument("Invalid Script reload snapshot");
    }
    std::unordered_set<std::string> availableModules;
    {
        Impl& candidate = *snapshot.candidate->impl_;
        const std::scoped_lock lock(impl_->mutex, candidate.mutex);
        if (!impl_->configured || impl_->mode != ScriptStoreMode::Loose ||
            !candidate.configured || candidate.mode != ScriptStoreMode::Loose ||
            candidate.runtimeRoot != impl_->runtimeRoot ||
            candidate.userDataRoot != impl_->userDataRoot ||
            candidate.reloadOwner != this ||
            candidate.reloadGeneration != impl_->generation) {
            throw std::runtime_error(
                "Script reload snapshot is no longer current");
        }
        availableModules.reserve(candidate.modules.size());
        for (const auto& [name, path] : snapshot.modules) {
            static_cast<void>(path);
            availableModules.emplace(name);
        }
        for (const auto& [name, path] : candidate.modules) {
            static_cast<void>(path);
            if (name.ends_with(
                    ludork::generated::resources::MetadataModuleSuffix)) {
                availableModules.emplace(name);
            }
        }
        impl_->entries = std::move(candidate.entries);
        impl_->modules = std::move(candidate.modules);
        impl_->orderedModules = std::move(candidate.orderedModules);
        ++impl_->generation;
        candidate.configured = false;
    }
    pushPreloadOwners(state, this, true);
    const int owners = lua_gettop(state);
    luaL_getsubtable(state, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    const int preloads = lua_gettop(state);
    lua_pushnil(state);
    while (lua_next(state, owners) != 0) {
        lua_pushvalue(state, -2);
        lua_rawget(state, preloads);
        const bool unchanged = lua_rawequal(state, -1, -2);
        const char* name = lua_tostring(state, -3);
        const bool removed =
            name != nullptr && !availableModules.contains(name);
        lua_pop(state, 1);
        if (unchanged && removed) {
            lua_pushvalue(state, -2);
            lua_pushnil(state);
            lua_rawset(state, preloads);
        }
        if (removed) {
            lua_pushvalue(state, -2);
            lua_pushnil(state);
            lua_rawset(state, owners);
        }
        lua_pop(state, 1);
    }
    lua_pop(state, 2);
    registerPreloadedModules(state);
}

ScriptStore& scriptStore() {
    static ScriptStore store;
    return store;
}

}  // namespace ludork::runtime
