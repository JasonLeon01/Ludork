#include "HotReloadImpl.hpp"
#include <ClassRuntimeProtocol.hpp>

#include "Blueprint/ClassRuntime/ClassRuntimeHotReload.hpp"

#include <ClassHotReload.hpp>
#include <LuaError.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace ludork::runtime {

namespace {

void rememberEnvironmentCopy(lua_State* state, int copiesIndex,
                             int originalIndex, int candidateIndex) {
    const int copies = lua_absindex(state, copiesIndex);
    const int original = lua_absindex(state, originalIndex);
    const int candidate = lua_absindex(state, candidateIndex);
    lua_pushvalue(state, original);
    lua_pushvalue(state, candidate);
    lua_rawset(state, copies);
}

void pushEnvironmentCopy(lua_State* state, int valueIndex, int copiesIndex,
                         const std::unordered_set<const void*>& nativeRoots) {
    if (lua_checkstack(state, 8) == 0) {
        throw std::runtime_error("Unable to copy the candidate environment");
    }
    const int value = lua_absindex(state, valueIndex);
    const int copies = lua_absindex(state, copiesIndex);
    if (!lua_istable(state, value)) {
        lua_pushvalue(state, value);
        return;
    }
    lua_pushvalue(state, value);
    lua_rawget(state, copies);
    if (!lua_isnil(state, -1)) {
        return;
    }
    lua_pop(state, 1);
    if (nativeRoots.contains(lua_topointer(state, value)) ||
        ludork::standard::class_runtime::isHotReloadClass(state, value)) {
        lua_pushvalue(state, value);
        return;
    }
    if (lua_getmetatable(state, value)) {
        lua_pop(state, 1);
        lua_pushvalue(state, value);
        return;
    }
    lua_newtable(state);
    const int candidate = lua_gettop(state);
    rememberEnvironmentCopy(state, copies, value, candidate);
    lua_pushnil(state);
    while (lua_next(state, value) != 0) {
        pushEnvironmentCopy(state, -2, copies, nativeRoots);
        pushEnvironmentCopy(state, -2, copies, nativeRoots);
        lua_rawset(state, candidate);
        lua_pop(state, 1);
    }
}

}  // namespace

HotReloadImpl::HotReloadImpl(lua_State* state)
    : state_(state), stackBase_(lua_gettop(state)) {
    lua_newtable(state_);
    referencesPointer_ = lua_topointer(state_, -1);
    references_ = luaL_ref(state_, LUA_REGISTRYINDEX);
    lua_pushglobaltable(state_);
    globals_ = keep(-1);
    lua_pop(state_, 1);
    lua_newtable(state_);
    candidateToLive_ = keep(-1);
    lua_pop(state_, 1);
}

HotReloadImpl::~HotReloadImpl() {
    lua_settop(state_, stackBase_);
    luaL_unref(state_, LUA_REGISTRYINDEX, references_);
}

int HotReloadImpl::keep(int index) {
    const int absolute = lua_absindex(state_, index);
    lua_rawgeti(state_, LUA_REGISTRYINDEX, references_);
    lua_pushvalue(state_, absolute);
    lua_rawseti(state_, -2, ++nextReference_);
    lua_pop(state_, 1);
    return nextReference_;
}

void HotReloadImpl::push(int reference) const {
    lua_rawgeti(state_, LUA_REGISTRYINDEX, references_);
    lua_rawgeti(state_, -1, reference);
    lua_remove(state_, -2);
}

int HotReloadImpl::getField(int reference, const char* name) {
    push(reference);
    lua_pushstring(state_, name);
    lua_rawget(state_, -2);
    const int result = keep(-1);
    lua_pop(state_, 2);
    return result;
}

int HotReloadImpl::getType(int reference) const {
    push(reference);
    const int result = lua_type(state_, -1);
    lua_pop(state_, 1);
    return result;
}

const void* HotReloadImpl::pointer(int reference) const {
    push(reference);
    const void* result = lua_topointer(state_, -1);
    lua_pop(state_, 1);
    return result;
}

bool HotReloadImpl::equal(int left, int right) const {
    push(left);
    push(right);
    const bool result = lua_rawequal(state_, -1, -2);
    lua_pop(state_, 2);
    return result;
}

std::string HotReloadImpl::stringValue(int reference) const {
    push(reference);
    std::size_t length = 0;
    const char* value = lua_tolstring(state_, -1, &length);
    const std::string result =
        value == nullptr ? "?" : std::string(value, length);
    lua_pop(state_, 1);
    return result;
}

std::vector<std::pair<int, int>> HotReloadImpl::entries(int reference) {
    std::vector<std::pair<int, int>> result;
    push(reference);
    const int table = lua_gettop(state_);
    lua_pushnil(state_);
    while (lua_next(state_, table) != 0) {
        result.emplace_back(keep(-2), keep(-1));
        lua_pop(state_, 1);
    }
    lua_pop(state_, 1);
    return result;
}

bool HotReloadImpl::isClass(int reference) const {
    push(reference);
    const bool result =
        ludork::standard::class_runtime::isHotReloadClass(state_, -1);
    lua_pop(state_, 1);
    return result;
}

bool HotReloadImpl::isLuaFunction(int reference) const {
    push(reference);
    const bool result =
        lua_isfunction(state_, -1) && !lua_iscfunction(state_, -1);
    lua_pop(state_, 1);
    return result;
}

bool HotReloadImpl::isClassInternal(int key) const {
    if (getType(key) != LUA_TSTRING) {
        return false;
    }
    const std::string name = stringValue(key);
    return name == "new" ||
           (name.starts_with("__") &&
            name != ludork::standard::class_runtime::protocol::
                        CLASS_GETTERS_FIELD &&
            name !=
                ludork::standard::class_runtime::protocol::CLASS_SETTERS_FIELD);
}

void HotReloadImpl::fail(const std::string& path,
                         const std::string& reason) const {
    throw std::runtime_error(path + ": " + reason + "; restart required");
}

void HotReloadImpl::run() {
    const char* editor = std::getenv("LUDORK_EDITOR");
    if (editor == nullptr || std::string_view(editor) != "1") {
        throw std::runtime_error(
            "$r is only available when launched by the editor");
    }
    const auto start = std::chrono::steady_clock::now();
    try {
        snapshot_ = scriptStore().prepareReload();
        phase_ = "compile";
        compile();
        phase_ = "candidate execution";
        loadCandidates();
        phase_ = "compatibility";
        preparePairs();
        prepareReferences();
        phase_ = "commit";
        commit();
        scriptStore().publishReload(state_, snapshot_);
    } catch (const std::exception& error) {
        std::string message = "[" + phase_ + "] " + error.what();
        if (phase_ == "candidate execution") {
            message +=
                "; definitions were not committed; top-level side "
                "effects may require a restart";
        } else if (phase_ == "commit") {
            message += "; commit could not finish, restart required";
        }
        throw std::runtime_error(message);
    }
    const double milliseconds = std::chrono::duration<double, std::milli>(
                                    std::chrono::steady_clock::now() - start)
                                    .count();
    std::cout << "INFO:Lua hot reload completed: " << moduleCount_
              << " modules, " << mixinCount_ << " Mixins, " << milliseconds
              << " ms. Existing data and constants were preserved."
              << std::endl;
}

void HotReloadImpl::compile() {
    for (const auto& [path, source] : snapshot_.sources) {
        if (luaL_loadbufferx(state_, source.data(), source.size(),
                             ("@" + path).c_str(), "t") != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state_, -1));
        }
        chunks_.emplace(path, keep(-1));
        lua_pop(state_, 1);
    }
}

int HotReloadImpl::requireCandidate(lua_State* state) {
    HotReloadImpl* impl =
        static_cast<HotReloadImpl*>(lua_touserdata(state, lua_upvalueindex(1)));
    try {
        if (lua_type(state, 1) != LUA_TSTRING) {
            throw std::runtime_error("require expects a module name");
        }
        const int result = impl->loadModule(lua_tostring(state, 1));
        impl->push(result);
        return 1;
    } catch (const std::exception& error) {
        lua_pushstring(state, error.what());
    }
    return lua_error(state);
}

int HotReloadImpl::execute(const std::string& path, const std::string& name) {
    const auto chunk = chunks_.find(path);
    if (chunk == chunks_.end()) {
        fail(path, "source is missing or is not a runtime Lua source");
    }
    const std::string& source = snapshot_.sources.at(path);
    if (luaL_loadbufferx(state_, source.data(), source.size(),
                         ("@" + path).c_str(), "t") != LUA_OK) {
        throw std::runtime_error(ludork::standard::luaErrorMessage(state_, -1));
    }
    push(environment_);
    if (lua_setupvalue(state_, -2, 1) == nullptr) {
        lua_pop(state_, 1);
    }
    lua_pushlstring(state_, name.data(), name.size());
    lua_pushlstring(state_, path.data(), path.size());
    if (ludork::standard::protectedLuaCall(state_, 2, 1) != LUA_OK) {
        throw std::runtime_error(path + ": " +
                                 ludork::standard::luaErrorMessage(state_, -1));
    }
    const int result = keep(-1);
    lua_pop(state_, 1);
    return result;
}

int HotReloadImpl::loadModule(const std::string& name) {
    const auto cached = modules_.find(name);
    if (cached != modules_.end()) {
        return cached->second;
    }
    if (loading_.contains(name)) {
        fail(name, "cyclic module initialization");
    }
    const auto source = snapshot_.modules.find(name);
    const int originalPackage = getField(globals_, "package");
    const int originalPreloads = getField(originalPackage, "preload");
    const bool occupied =
        liveModules_.contains(name) ||
        getType(getField(originalPreloads, name.c_str())) != LUA_TNIL;
    const bool project = source != snapshot_.modules.end() &&
                         (scriptStore().ownsModule(state_, name) || !occupied);
    int result;
    if (project) {
        if (name == "Entry" || name.ends_with("_meta")) {
            fail(name, "entry and metadata cannot execute during hot reload");
        }
        loading_.insert(name);
        result = execute(source->second, name);
        loading_.erase(name);
        if (getType(result) == LUA_TNIL) {
            fail(name, "reloadable modules must return their definition");
        }
    } else {
        if (scriptStore().ownsModule(state_, name)) {
            fail(name, "loaded project module was removed");
        }
        const int require = getField(globals_, "require");
        push(require);
        lua_pushlstring(state_, name.data(), name.size());
        if (ludork::standard::protectedLuaCall(state_, 1, 1) != LUA_OK) {
            throw std::runtime_error(
                ludork::standard::luaErrorMessage(state_, -1));
        }
        result = keep(-1);
        lua_pop(state_, 1);
    }
    modules_.emplace(name, result);
    push(loaded_);
    push(result);
    lua_setfield(state_, -2, name.c_str());
    lua_pop(state_, 1);
    return result;
}

void HotReloadImpl::loadCandidates() {
    const int originalPackage = getField(globals_, "package");
    const int originalLoaded = getField(originalPackage, "loaded");
    const int originalPreloads = getField(originalPackage, "preload");
    std::unordered_set<const void*> nativeRoots;
    for (const auto& [key, value] : entries(originalLoaded)) {
        if (getType(key) == LUA_TSTRING && getType(value) != LUA_TNIL) {
            const std::string name = stringValue(key);
            liveModules_.emplace(name, value);
            if (name != "_G" && name != "package" &&
                !scriptStore().ownsModule(state_, name) &&
                getType(value) == LUA_TTABLE) {
                nativeRoots.insert(pointer(value));
            }
        }
    }
    const int sf = getField(globals_, "sf");
    if (getType(sf) == LUA_TTABLE) {
        nativeRoots.insert(pointer(sf));
    }
    lua_newtable(state_);
    environment_ = keep(-1);
    lua_pop(state_, 1);
    lua_newtable(state_);
    package_ = keep(-1);
    lua_pop(state_, 1);
    lua_newtable(state_);
    loaded_ = keep(-1);
    lua_pop(state_, 1);
    lua_newtable(state_);
    const int preloads = keep(-1);
    lua_pop(state_, 1);
    lua_newtable(state_);
    const int copies = lua_gettop(state_);
    for (const auto& [original, candidate] :
         {std::pair{globals_, environment_},
          std::pair{originalPackage, package_},
          std::pair{originalLoaded, loaded_},
          std::pair{originalPreloads, preloads}}) {
        push(original);
        push(candidate);
        rememberEnvironmentCopy(state_, copies, -2, -1);
        lua_pop(state_, 2);
    }
    for (const auto& [key, value] : entries(originalPreloads)) {
        push(preloads);
        push(key);
        push(value);
        lua_rawset(state_, -3);
        lua_pop(state_, 1);
    }
    for (const auto& [key, value] : entries(globals_)) {
        push(environment_);
        push(key);
        pushEnvironmentCopy(state_, -1, copies, nativeRoots);
        lua_remove(state_, -2);
        push(value);
        pushEnvironmentCopy(state_, -1, copies, nativeRoots);
        lua_remove(state_, -2);
        lua_rawset(state_, -3);
        lua_pop(state_, 1);
    }
    for (const auto& [key, value] : entries(originalPackage)) {
        push(package_);
        push(key);
        pushEnvironmentCopy(state_, -1, copies, nativeRoots);
        lua_remove(state_, -2);
        push(value);
        pushEnvironmentCopy(state_, -1, copies, nativeRoots);
        lua_remove(state_, -2);
        lua_rawset(state_, -3);
        lua_pop(state_, 1);
    }
    lua_pop(state_, 1);
    push(package_);
    push(loaded_);
    lua_setfield(state_, -2, "loaded");
    lua_pop(state_, 1);
    push(environment_);
    push(environment_);
    lua_setfield(state_, -2, "_G");
    push(package_);
    lua_setfield(state_, -2, "package");
    lua_pushlightuserdata(state_, this);
    lua_pushcclosure(state_, requireCandidate, 1);
    lua_setfield(state_, -2, "require");
    lua_pop(state_, 1);
    for (const auto& [name, live] : liveModules_) {
        if (scriptStore().ownsModule(state_, name) && name != "Entry" &&
            !name.ends_with("_meta")) {
            const int candidate = loadModule(name);
            roots_.push_back({live, candidate, name});
            ++moduleCount_;
        }
    }
    class_runtime_detail::pushHotReloadMixins(state_);
    mixins_ = keep(-1);
    lua_pop(state_, 1);
    for (const auto& [key, entry] : entries(mixins_)) {
        static_cast<void>(key);
        const std::string path = stringValue(getField(entry, "scriptPath"));
        const int candidate =
            execute(path, stringValue(getField(entry, "modulePath")));
        const int live = getField(entry, "definition");
        const int type = getField(entry, "class");
        push(type);
        push(candidate);
        class_runtime_detail::validateHotReloadMixin(state_, -2, -1);
        lua_pop(state_, 2);
        roots_.push_back({live, candidate, path});
        mixinClasses_.push_back({type, candidate, path});
        ++mixinCount_;
    }
}

}  // namespace ludork::runtime
