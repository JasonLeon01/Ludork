#include <Runtime/ScriptStore.hpp>

#include "LdPakArchive.hpp"
#include <Utf8Path.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <fstream>
#include <iterator>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

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
    return path.ends_with(".lua") || path.ends_with(".luac");
}

std::string moduleName(const std::string& path) {
    std::size_t extensionSize = 0;
    if (path.ends_with(".luac")) {
        extensionSize = 5;
    } else if (path.ends_with(".lua")) {
        extensionSize = 4;
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
    constexpr std::string_view Prefix = "Scripts/";
    if (!scriptPath.starts_with(Prefix) || scriptPath.size() == Prefix.size() ||
        scriptPath.find('\\') != std::string::npos ||
        scriptPath.find('\0') != std::string::npos ||
        (!scriptPath.ends_with(".lua") && !scriptPath.ends_with(".luac"))) {
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

}  // namespace

namespace ludork::runtime {

namespace {

struct ScriptEntry {
    std::filesystem::path source;
    std::string archivePath;
};

void addScriptEntry(std::unordered_map<std::string, ScriptEntry>& entries,
                    std::unordered_map<std::string, std::string>& foldedPaths,
                    const std::string& relative, ScriptEntry entry) {
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
    const bool newSource = relative.ends_with(".lua");
    const bool oldSource = existing->second.ends_with(".lua");
    if (newSource == oldSource) {
        throw std::runtime_error("Multiple Script files map to module " + name);
    }
    if (newSource) {
        existing->second = relative;
    }
}

std::string alternateScriptPath(const std::string& relative) {
    if (relative.ends_with(".lua")) {
        return relative + 'c';
    }
    if (relative.ends_with(".luac")) {
        return relative.substr(0, relative.size() - 1);
    }
    return {};
}

int preloadScript(lua_State* state) {
    ScriptStore* store =
        static_cast<ScriptStore*>(lua_touserdata(state, lua_upvalueindex(1)));
    std::size_t moduleLength = 0;
    const char* moduleValue =
        lua_tolstring(state, lua_upvalueindex(2), &moduleLength);
    int status = LUA_ERRFILE;
    try {
        if (store == nullptr || moduleValue == nullptr) {
            lua_pushliteral(state, "Invalid Script preload closure");
        } else {
            status = store->loadModule(state,
                                       std::string(moduleValue, moduleLength));
        }
    } catch (const std::exception& exception) {
        lua_pushstring(state, exception.what());
    }
    if (status != LUA_OK) {
        return lua_error(state);
    }
    const int argumentCount = lua_gettop(state) - 1;
    lua_insert(state, 1);
    status = lua_pcall(state, argumentCount, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        return lua_error(state);
    }
    return lua_gettop(state);
}

}  // namespace

struct ScriptStore::Impl {
    mutable std::shared_mutex mutex;
    std::filesystem::path runtimeRoot;
    ScriptStoreMode mode = ScriptStoreMode::Loose;
    bool configured = false;
    std::shared_ptr<detail::LdPakArchive> archive;
    std::unordered_map<std::string, ScriptEntry> entries;
    std::unordered_map<std::string, std::string> modules;
    std::vector<std::string> orderedModules;
};

ScriptStore::ScriptStore() : impl_(std::make_unique<Impl>()) {}
ScriptStore::~ScriptStore() = default;

void ScriptStore::configure(const std::filesystem::path& runtimeRoot) {
    std::error_code error;
    const std::filesystem::path normalized =
        std::filesystem::weakly_canonical(runtimeRoot, error);
    if (error || normalized.empty()) {
        throw std::invalid_argument("Invalid runtime root for ScriptStore");
    }
    const std::filesystem::path scriptsRoot = normalized / "Scripts";
    const std::filesystem::path packagePath = normalized / "Scripts.ldpak";
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
            "Runtime root must contain exactly one of Scripts or "
            "Scripts.ldpak");
    }

    std::unordered_map<std::string, ScriptEntry> loadedEntries;
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
        if (loadedArchive->group() != "Scripts") {
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
    if (!loadedEntries.contains("Entry.lua") &&
        !loadedEntries.contains("Entry.luac")) {
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
        auto entry = impl_->entries.find(relative);
        if (entry == impl_->entries.end()) {
            entry = impl_->entries.find(alternateScriptPath(relative));
        }
        if (entry == impl_->entries.end()) {
            throw std::runtime_error("Script file not found: " + scriptPath);
        }
        const std::vector<std::uint8_t> source =
            impl_->archive ? impl_->archive->readAll(entry->second.archivePath)
                           : readPhysicalFile(entry->second.source);
        const std::string chunkName = "@Scripts/" + entry->first;
        const char* sourceData =
            source.empty() ? "" : reinterpret_cast<const char*>(source.data());
        return luaL_loadbufferx(state, sourceData, source.size(),
                                chunkName.c_str(), nullptr);
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
    std::string relative;
    {
        std::shared_lock lock(impl_->mutex);
        if (!impl_->configured) {
            lua_pushliteral(state, "ScriptStore is not configured");
            return LUA_ERRFILE;
        }
        const auto module = impl_->modules.find(moduleName);
        if (module == impl_->modules.end()) {
            lua_pushstring(state,
                           ("Script module not found: " + moduleName).c_str());
            return LUA_ERRFILE;
        }
        relative = module->second;
    }
    return loadFile(state, "Scripts/" + relative);
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
        lua_pushcclosure(state, preloadScript, 2);
        lua_setfield(state, preloadIndex, name.c_str());
    }
    lua_pop(state, 2);
}

ScriptStore& scriptStore() {
    static ScriptStore store;
    return store;
}

}  // namespace ludork::runtime
