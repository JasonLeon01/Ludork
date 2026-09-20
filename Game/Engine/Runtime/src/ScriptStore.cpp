#include <LuaError.hpp>
#include <Runtime/ScriptStore.hpp>
#include <LudorkGenerated/ResourceFileConstants.hpp>

#include "ScriptStoreImpl.hpp"
#include "ScriptModuleShape.hpp"
#include "LdPakArchive.hpp"
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
#include <shared_mutex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ludork::runtime {
namespace {

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

std::string asciiFold(std::string value) {
    for (char& character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return value;
}

bool isLinkLike(const std::filesystem::path& path,
                const std::filesystem::file_status& status) {
    if (std::filesystem::is_symlink(status)) {
        return true;
    }
#if defined(_WIN32)
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        throw std::runtime_error("Failed to inspect Scripts filesystem entry");
    }
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    static_cast<void>(path);
    return false;
#endif
}

bool isDeclarationPath(const std::string_view path) {
    const std::string folded = asciiFold(std::string(path));
    return folded == "stub" || folded.starts_with("stub/") ||
           folded.ends_with(".d.lua");
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
    if (!scriptPath.starts_with(Prefix) || scriptPath.size() == Prefix.size() ||
        scriptPath.find('\\') != std::string::npos ||
        scriptPath.find('\0') != std::string::npos ||
        (!scriptPath.ends_with(
             ludork::generated::resources::LuaSourceExtension) &&
         !scriptPath.ends_with(
             ludork::generated::resources::LuaCompiledExtension))) {
        throw std::invalid_argument(
            "Script path must name a .lua or .luac file under Scripts");
    }
    const std::string relative = scriptPath.substr(Prefix.size());
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
    const std::string folded = asciiFold(relative);
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

int loadScriptEntry(
    lua_State* state, const std::string& relative,
    const std::unordered_map<std::string, script_store_impl::ScriptEntry>&
        entries,
    const std::shared_ptr<detail::LdPakArchive>& archive) {
    auto entry = entries.find(relative);
    if (entry == entries.end()) {
        entry = entries.find(alternateScriptPath(relative));
    }
    if (entry == entries.end()) {
        throw std::runtime_error("Script file not found: Scripts/" + relative);
    }
    const std::vector<std::uint8_t> source =
        archive ? archive->readAll(entry->second.archivePath)
                : readPhysicalFile(entry->second.source);
    const std::string chunkName = "@Scripts/" + entry->first;
    const char* sourceData =
        source.empty() ? "" : reinterpret_cast<const char*>(source.data());
    return luaL_loadbufferx(state, sourceData, source.size(), chunkName.c_str(),
                            nullptr);
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
            isLinkLike(scriptsRoot, scriptsStatus)) {
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
            if (isLinkLike(entry.path(), status)) {
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
                           {loadedArchive->path(), entry.path});
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
    impl_->mode = loadedMode;
    impl_->archive = std::move(loadedArchive);
    impl_->entries = std::move(loadedEntries);
    impl_->modules = std::move(loadedModules);
    impl_->orderedModules = std::move(orderedModules);
    impl_->configured = true;
    ++impl_->generation;
    impl_->reloadOwner = nullptr;
}

void ScriptStore::reset() noexcept {
    std::unique_lock lock(impl_->mutex);
    impl_->runtimeRoot.clear();
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

int ScriptStore::loadFile(lua_State* state,
                          const std::string& scriptPath) const {
    if (state == nullptr) {
        throw std::invalid_argument("Lua state must not be null");
    }
    try {
        const std::string relative = validateScriptPath(scriptPath);
        std::shared_lock lock(impl_->mutex);
        if (!impl_->configured) {
            throw std::logic_error("ScriptStore is not configured");
        }
        return loadScriptEntry(state, relative, impl_->entries, impl_->archive);
    } catch (const std::exception& exception) {
        lua_pushstring(state, exception.what());
        return LUA_ERRFILE;
    }
}

int ScriptStore::loadModule(lua_State* state,
                            const std::string& moduleName) const {
    if (state == nullptr) {
        throw std::invalid_argument("Lua state must not be null");
    }
    try {
        std::shared_lock lock(impl_->mutex);
        if (!impl_->configured) {
            throw std::logic_error("ScriptStore is not configured");
        }
        const auto module = impl_->modules.find(moduleName);
        if (module == impl_->modules.end()) {
            throw std::runtime_error("Script module not found: " + moduleName);
        }
        const std::string relative = validateScriptPath(
            ludork::generated::resources::ScriptPathPrefix + module->second);
        return loadScriptEntry(state, relative, impl_->entries, impl_->archive);
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
    const bool captureDefinitions =
        editor != nullptr && std::string_view(editor) == "1";
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
        lua_pushlightuserdata(state, const_cast<ScriptStore*>(this));
        lua_pushlstring(state, name.data(), name.size());
        lua_pushboolean(
            state, captureDefinitions &&
                       impl_->mode == ScriptStoreMode::Loose &&
                       impl_->modules.at(name).ends_with(
                           ludork::generated::resources::LuaSourceExtension) &&
                       !name.ends_with("_meta"));
        lua_pushcclosure(state, preloadScript, 3);
        lua_pushvalue(state, -1);
        lua_setfield(state, ownersIndex, name.c_str());
        lua_setfield(state, preloadIndex, name.c_str());
    }
    lua_pop(state, 3);
}

ScriptStore::ReloadSnapshot ScriptStore::prepareReload() const {
    ReloadSnapshot snapshot;
    std::filesystem::path root;
    std::uint64_t generation = 0;
    {
        std::shared_lock lock(impl_->mutex);
        if (!impl_->configured || impl_->mode != ScriptStoreMode::Loose) {
            throw std::runtime_error(
                "Hot reload requires loose Lua source files");
        }
        root = impl_->runtimeRoot;
        generation = impl_->generation;
    }
    snapshot.candidate = std::make_unique<ScriptStore>();
    snapshot.candidate->configure(root);
    Impl& candidate = *snapshot.candidate->impl_;
    if (candidate.mode != ScriptStoreMode::Loose) {
        throw std::runtime_error(
            "Hot reload cannot switch script storage mode");
    }
    candidate.reloadOwner = this;
    candidate.reloadGeneration = generation;
    for (const auto& [path, entry] : candidate.entries) {
        if (path.ends_with("_meta.lua") || path.ends_with("_meta.luac")) {
            continue;
        }
        if (!path.ends_with(ludork::generated::resources::LuaSourceExtension)) {
            if (candidate.entries.contains(alternateScriptPath(path))) {
                continue;
            }
            throw std::runtime_error(
                "Hot reload does not support bytecode: Scripts/" + path);
        }
        const std::vector<std::uint8_t> bytes = readPhysicalFile(entry.source);
        snapshot.sources.emplace(
            ludork::generated::resources::ScriptPathPrefix + path,
            std::string(bytes.begin(), bytes.end()));
    }
    for (const auto& [name, path] : candidate.modules) {
        if (snapshot.sources.contains(
                ludork::generated::resources::ScriptPathPrefix + path)) {
            snapshot.modules.emplace(
                name, ludork::generated::resources::ScriptPathPrefix + path);
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
            candidate.reloadOwner != this ||
            candidate.reloadGeneration != impl_->generation) {
            throw std::runtime_error(
                "Script reload snapshot is no longer current");
        }
        availableModules.reserve(candidate.modules.size());
        for (const auto& [name, path] : candidate.modules) {
            static_cast<void>(path);
            availableModules.emplace(name);
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
