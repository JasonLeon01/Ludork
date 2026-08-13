#include <RuntimeSession.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace ludork::standard {

namespace {

std::mutex sessionMutex;
std::shared_ptr<RuntimeSessionState> activeSession;

struct ThreadSessionEntry {
    lua_State* state = nullptr;
    std::shared_ptr<RuntimeSessionState> session;
    std::size_t depth = 0;
};

thread_local ThreadSessionEntry enteredSession;

bool sessionAllowsCurrentThread(const RuntimeSessionState& session) noexcept {
    const RuntimeSessionPhase phase =
        session.phase.load(std::memory_order_acquire);
    return phase == RuntimeSessionPhase::running ||
           (phase == RuntimeSessionPhase::stopping &&
            session.shutdownThread == std::this_thread::get_id());
}

std::shared_ptr<RuntimeSessionState> findSession(lua_State* state) {
    std::scoped_lock lock(sessionMutex);
    if (activeSession == nullptr || activeSession->state != state) {
        return {};
    }
    return activeSession;
}

std::shared_ptr<RuntimeSessionState> tryFindSession(lua_State* state) {
    std::unique_lock lock(sessionMutex, std::try_to_lock);
    if (!lock.owns_lock() || activeSession == nullptr ||
        activeSession->state != state) {
        return {};
    }
    return activeSession;
}

int enterSession(lua_State* state, bool blocking) noexcept {
    try {
        const std::shared_ptr<RuntimeSessionState> session =
            blocking ? findSession(state) : tryFindSession(state);
        if (session == nullptr) {
            return 0;
        }
        if (enteredSession.depth != 0 && enteredSession.state != state) {
            return 0;
        }
        if (blocking) {
            session->luaMutex.lock();
        } else if (!session->luaMutex.try_lock()) {
            return 0;
        }
        if (session->state != state || !sessionAllowsCurrentThread(*session)) {
            session->luaMutex.unlock();
            return 0;
        }
        if (enteredSession.depth == 0) {
            enteredSession.state = state;
            enteredSession.session = session;
        }
        ++enteredSession.depth;
        return 1;
    } catch (...) {
        return 0;
    }
}

}  // namespace

struct RuntimeRegistryReferenceState {
    RuntimeRegistryReferenceState(std::shared_ptr<RuntimeSessionState> session,
                                  lua_State* state, int reference)
        : session(std::move(session)), state(state), reference(reference) {}

    ~RuntimeRegistryReferenceState() {
        std::scoped_lock luaLock(session->luaMutex);
        if (reference >= 0 && session->state == state &&
            sessionAllowsCurrentThread(*session)) {
            luaL_unref(state, LUA_REGISTRYINDEX, reference);
            reference = LUA_NOREF;
        }
        session->registryReferences.erase(this);
    }

    std::shared_ptr<RuntimeSessionState> session;
    lua_State* state;
    int reference;
};

LuaExecutionScope::LuaExecutionScope(lua_State* state)
    : state_(state), active_(enterRuntimeSession(state) != 0) {}

LuaExecutionScope::~LuaExecutionScope() {
    if (active_) {
        leaveRuntimeSession(state_);
    }
}

bool LuaExecutionScope::active() const noexcept {
    return active_;
}

LuaExecutionPause::LuaExecutionPause() noexcept {
    if (enteredSession.depth == 0 || enteredSession.session == nullptr) {
        return;
    }
    state_ = enteredSession.state;
    session_ = enteredSession.session;
    depth_ = enteredSession.depth;
    enteredSession.state = nullptr;
    enteredSession.session.reset();
    enteredSession.depth = 0;
    for (std::size_t index = 0; index < depth_; ++index) {
        session_->luaMutex.unlock();
    }
}

LuaExecutionPause::~LuaExecutionPause() {
    if (session_ == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < depth_; ++index) {
        session_->luaMutex.lock();
    }
    enteredSession.state = state_;
    enteredSession.session = std::move(session_);
    enteredSession.depth = depth_;
}

LuaRegistryReference::LuaRegistryReference(lua_State* state, int stackIndex) {
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    if (session == nullptr) {
        throw std::logic_error("Lua registry reference has no runtime session");
    }
    std::shared_ptr<RuntimeRegistryReferenceState> reference =
        std::make_shared<RuntimeRegistryReferenceState>(session, state,
                                                        LUA_NOREF);
    std::scoped_lock luaLock(session->luaMutex);
    if (!sessionAllowsCurrentThread(*session)) {
        throw std::logic_error("Lua runtime session is stopping");
    }
    session->registryReferences.insert(reference.get());
    lua_pushvalue(state, stackIndex);
    reference->reference = luaL_ref(state, LUA_REGISTRYINDEX);
    reference_ = std::move(reference);
}

lua_State* LuaRegistryReference::state() const noexcept {
    return reference_ == nullptr ? nullptr : reference_->state;
}

bool LuaRegistryReference::push() const noexcept {
    if (reference_ == nullptr) {
        return false;
    }
    const std::shared_ptr<RuntimeSessionState>& session = reference_->session;
    std::scoped_lock luaLock(session->luaMutex);
    if (reference_->reference < 0 || session->state != reference_->state ||
        !sessionAllowsCurrentThread(*session)) {
        return false;
    }
    lua_rawgeti(reference_->state, LUA_REGISTRYINDEX, reference_->reference);
    return true;
}

bool LuaRegistryReference::equals(
    const LuaRegistryReference& other) const noexcept {
    if (reference_ == nullptr || other.reference_ == nullptr ||
        reference_->session != other.reference_->session ||
        reference_->state != other.reference_->state) {
        return false;
    }
    const std::shared_ptr<RuntimeSessionState>& session = reference_->session;
    std::scoped_lock luaLock(session->luaMutex);
    if (reference_->reference < 0 || other.reference_->reference < 0 ||
        session->state != reference_->state ||
        !sessionAllowsCurrentThread(*session)) {
        return false;
    }
    lua_rawgeti(reference_->state, LUA_REGISTRYINDEX, reference_->reference);
    lua_rawgeti(reference_->state, LUA_REGISTRYINDEX,
                other.reference_->reference);
    const bool result = lua_rawequal(reference_->state, -2, -1) != 0;
    lua_pop(reference_->state, 2);
    return result;
}

LuaRegistryReference::operator bool() const noexcept {
    return reference_ != nullptr;
}

void registerRuntimeOpaqueValue(const void* identity,
                                const LuaRegistryReference& reference) {
    if (identity == nullptr || !reference) {
        throw std::invalid_argument("Runtime opaque value is unavailable");
    }
    lua_State* state = reference.state();
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    if (session == nullptr) {
        throw std::logic_error("Runtime opaque value has no active session");
    }
    std::scoped_lock luaLock(session->luaMutex);
    if (session->state != state || !sessionAllowsCurrentThread(*session)) {
        throw std::logic_error("Lua runtime session is stopping");
    }
    session->opaqueValues[identity] = &reference;
}

void unregisterRuntimeOpaqueValue(lua_State* state,
                                  const void* identity) noexcept {
    if (state == nullptr || identity == nullptr) {
        return;
    }
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    if (session == nullptr) {
        return;
    }
    std::scoped_lock luaLock(session->luaMutex);
    session->opaqueValues.erase(identity);
}

LuaRegistryReference findRuntimeOpaqueValue(lua_State* state,
                                            const void* identity) {
    if (state == nullptr || identity == nullptr) {
        return {};
    }
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    if (session == nullptr) {
        return {};
    }
    std::scoped_lock luaLock(session->luaMutex);
    if (session->state != state || !sessionAllowsCurrentThread(*session)) {
        return {};
    }
    const auto iterator = session->opaqueValues.find(identity);
    if (iterator == session->opaqueValues.end() ||
        iterator->second == nullptr) {
        return {};
    }
    return *iterator->second;
}

void initializeRuntimeSession(lua_State* state) {
    if (state == nullptr) {
        throw std::invalid_argument("Runtime session state cannot be null");
    }
    std::scoped_lock lock(sessionMutex);
    if (activeSession != nullptr) {
        if (activeSession->state == state &&
            activeSession->phase.load(std::memory_order_acquire) ==
                RuntimeSessionPhase::running) {
            return;
        }
        throw std::logic_error("Only one Lua runtime session may be active");
    }
    activeSession = std::make_shared<RuntimeSessionState>(state);
}

int enterRuntimeSession(lua_State* state) noexcept {
    return enterSession(state, true);
}

int tryEnterRuntimeSession(lua_State* state) noexcept {
    return enterSession(state, false);
}

void leaveRuntimeSession(lua_State* state) noexcept {
    if (enteredSession.depth == 0 || enteredSession.state != state) {
        return;
    }
    const std::shared_ptr<RuntimeSessionState> session = enteredSession.session;
    --enteredSession.depth;
    session->luaMutex.unlock();
    if (enteredSession.depth == 0) {
        enteredSession.state = nullptr;
        enteredSession.session.reset();
    }
}

void beginRuntimeShutdown(lua_State* state) noexcept {
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    if (session == nullptr) {
        return;
    }
    session->phase.store(RuntimeSessionPhase::stopping,
                         std::memory_order_release);
    std::scoped_lock luaLock(session->luaMutex);
    session->shutdownThread = std::this_thread::get_id();
}

void registerRuntimeCleanup(lua_State* state, RuntimeCleanup cleanup) {
    if (cleanup == nullptr) {
        throw std::invalid_argument("Runtime cleanup cannot be null");
    }
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    if (session == nullptr) {
        throw std::logic_error("Runtime cleanup has no active session");
    }
    std::scoped_lock luaLock(session->luaMutex);
    if (session->state != state ||
        session->phase.load(std::memory_order_acquire) !=
            RuntimeSessionPhase::running) {
        throw std::logic_error("Lua runtime session is stopping");
    }
    if (std::find(session->moduleCleanups.begin(),
                  session->moduleCleanups.end(),
                  cleanup) == session->moduleCleanups.end()) {
        session->moduleCleanups.push_back(cleanup);
    }
}

void runRuntimeCleanups(lua_State* state) noexcept {
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    if (session == nullptr) {
        return;
    }
    std::vector<RuntimeCleanup> cleanups;
    {
        std::scoped_lock luaLock(session->luaMutex);
        if (session->state != state || !sessionAllowsCurrentThread(*session)) {
            return;
        }
        cleanups.swap(session->moduleCleanups);
    }
    for (auto cleanup = cleanups.rbegin(); cleanup != cleanups.rend();
         ++cleanup) {
        (*cleanup)(state);
    }
}

void clearRuntimeRegistryReferences(lua_State* state) noexcept {
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    if (session == nullptr) {
        return;
    }
    std::scoped_lock luaLock(session->luaMutex);
    if (session->state != state || !sessionAllowsCurrentThread(*session)) {
        return;
    }
    session->opaqueValues.clear();
    for (RuntimeRegistryReferenceState* reference :
         session->registryReferences) {
        if (reference == nullptr || reference->reference < 0) {
            continue;
        }
        luaL_unref(state, LUA_REGISTRYINDEX, reference->reference);
        reference->reference = LUA_NOREF;
    }
    session->registryReferences.clear();
}

void releaseRuntimeSession(lua_State* state) noexcept {
    clearRuntimeRegistryReferences(state);
    std::shared_ptr<RuntimeSessionState> session;
    {
        std::scoped_lock lock(sessionMutex);
        if (activeSession == nullptr || activeSession->state != state) {
            return;
        }
        session = std::move(activeSession);
    }
    std::scoped_lock luaLock(session->luaMutex);
    session->phase.store(RuntimeSessionPhase::stopped,
                         std::memory_order_release);
    session->shutdownThread = {};
    session->state = nullptr;
}

bool isRuntimeStopping(lua_State* state) noexcept {
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    return session == nullptr ||
           session->phase.load(std::memory_order_acquire) !=
               RuntimeSessionPhase::running;
}

}  // namespace ludork::standard
