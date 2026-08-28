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

lua_State* mainThreadFromRegistry(lua_State* state) noexcept {
    if (state == nullptr || lua_checkstack(state, 1) == 0) {
        return nullptr;
    }
    const int originalTop = lua_gettop(state);
    lua_rawgeti(state, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    lua_State* mainState = lua_tothread(state, -1);
    lua_settop(state, originalTop);
    return mainState;
}

bool isEnteredSession(
    const std::shared_ptr<RuntimeSessionState>& session) noexcept {
    return enteredSession.depth != 0 && enteredSession.session == session &&
           enteredSession.state == session->state;
}

bool sessionAllowsCurrentThread(const RuntimeSessionState& session) noexcept {
    const RuntimeSessionPhase phase =
        session.phase.load(std::memory_order_acquire);
    return phase == RuntimeSessionPhase::running ||
           (phase == RuntimeSessionPhase::stopping &&
            session.shutdownThread == std::this_thread::get_id());
}

std::shared_ptr<RuntimeSessionState> findSession(lua_State* state) {
    std::shared_ptr<RuntimeSessionState> session;
    {
        std::scoped_lock lock(sessionMutex);
        session = activeSession;
    }
    if (session == nullptr) {
        return {};
    }
    if (session->state != state) {
        std::scoped_lock luaLock(session->luaMutex);
        if (session->state != mainThreadFromRegistry(state)) {
            return {};
        }
    }
    return session;
}

std::shared_ptr<RuntimeSessionState> tryFindSession(lua_State* state) {
    std::shared_ptr<RuntimeSessionState> session;
    std::unique_lock lock(sessionMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        return {};
    }
    session = activeSession;
    lock.unlock();
    if (session == nullptr) {
        return {};
    }
    if (session->state != state) {
        std::unique_lock luaLock(session->luaMutex, std::try_to_lock);
        if (!luaLock.owns_lock() ||
            session->state != mainThreadFromRegistry(state)) {
            return {};
        }
    }
    return session;
}

int enterSession(lua_State* state, bool blocking) noexcept {
    try {
        if (enteredSession.depth != 0) {
            if (enteredSession.session == nullptr ||
                enteredSession.session->state != enteredSession.state ||
                (enteredSession.state != state &&
                 enteredSession.state != mainThreadFromRegistry(state)) ||
                !sessionAllowsCurrentThread(*enteredSession.session)) {
                return 0;
            }
            ++enteredSession.depth;
            return 1;
        }
        const std::shared_ptr<RuntimeSessionState> session =
            blocking ? findSession(state) : tryFindSession(state);
        if (session == nullptr) {
            return 0;
        }
        if (blocking) {
            session->luaMutex.lock();
        } else if (!session->luaMutex.try_lock()) {
            return 0;
        }
        if (session->state == nullptr ||
            !sessionAllowsCurrentThread(*session)) {
            session->luaMutex.unlock();
            return 0;
        }
        enteredSession.state = session->state;
        enteredSession.session = session;
        enteredSession.depth = 1;
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
        const auto release = [this]() {
            if (reference >= 0 && session->state == state &&
                sessionAllowsCurrentThread(*session)) {
                luaL_unref(state, LUA_REGISTRYINDEX, reference);
                reference = LUA_NOREF;
            }
            session->registryReferences.erase(this);
        };
        if (isEnteredSession(session)) {
            release();
        } else {
            std::scoped_lock luaLock(session->luaMutex);
            release();
        }
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
    session_->luaMutex.unlock();
}

LuaExecutionPause::~LuaExecutionPause() {
    if (session_ == nullptr) {
        return;
    }
    session_->luaMutex.lock();
    enteredSession.state = state_;
    enteredSession.session = std::move(session_);
    enteredSession.depth = depth_;
}

LuaRegistryReference::LuaRegistryReference(lua_State* state, int stackIndex) {
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    if (session == nullptr) {
        throw std::logic_error("Lua registry reference has no runtime session");
    }
    lua_State* canonicalState = session->state;
    std::shared_ptr<RuntimeRegistryReferenceState> reference =
        std::make_shared<RuntimeRegistryReferenceState>(session, canonicalState,
                                                        LUA_NOREF);
    const auto create = [&]() {
        if (session->state != canonicalState ||
            !sessionAllowsCurrentThread(*session)) {
            throw std::logic_error("Lua runtime session is stopping");
        }
        session->registryReferences.insert(reference.get());
        lua_pushvalue(state, stackIndex);
        reference->reference = luaL_ref(state, LUA_REGISTRYINDEX);
    };
    if (isEnteredSession(session)) {
        create();
    } else {
        std::scoped_lock luaLock(session->luaMutex);
        create();
    }
    reference_ = std::move(reference);
}

lua_State* LuaRegistryReference::state() const noexcept {
    return reference_ == nullptr ? nullptr : reference_->state;
}

bool LuaRegistryReference::push() const noexcept {
    if (enteredSession.depth != 0) {
        return pushUnderExecutionScope();
    }
    if (reference_ == nullptr) {
        return false;
    }
    const std::shared_ptr<RuntimeSessionState>& session = reference_->session;
    std::scoped_lock luaLock(session->luaMutex);
    if (reference_->reference < 0 || session->state != reference_->state ||
        !sessionAllowsCurrentThread(*session) ||
        lua_checkstack(reference_->state, 1) == 0) {
        return false;
    }
    lua_rawgeti(reference_->state, LUA_REGISTRYINDEX, reference_->reference);
    return true;
}

bool LuaRegistryReference::pushUnderExecutionScope() const noexcept {
    return pushUnderExecutionScope(state());
}

bool LuaRegistryReference::pushUnderExecutionScope(
    lua_State* targetState) const noexcept {
    if (reference_ == nullptr || reference_->reference < 0 ||
        targetState == nullptr || enteredSession.depth == 0 ||
        enteredSession.session != reference_->session ||
        reference_->session->state != reference_->state ||
        !sessionAllowsCurrentThread(*reference_->session) ||
        (targetState != reference_->state &&
         mainThreadFromRegistry(targetState) != reference_->state) ||
        lua_checkstack(targetState, 1) == 0) {
        return false;
    }
    lua_rawgeti(targetState, LUA_REGISTRYINDEX, reference_->reference);
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
    const auto compare = [&]() {
        if (reference_->reference < 0 || other.reference_->reference < 0 ||
            session->state != reference_->state ||
            !sessionAllowsCurrentThread(*session)) {
            return false;
        }
        lua_rawgeti(reference_->state, LUA_REGISTRYINDEX,
                    reference_->reference);
        lua_rawgeti(reference_->state, LUA_REGISTRYINDEX,
                    other.reference_->reference);
        const bool result = lua_rawequal(reference_->state, -2, -1) != 0;
        lua_pop(reference_->state, 2);
        return result;
    };
    if (isEnteredSession(session)) {
        return compare();
    }
    std::scoped_lock luaLock(session->luaMutex);
    return compare();
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
    const auto registerValue = [&]() {
        if (session->state != state || !sessionAllowsCurrentThread(*session)) {
            throw std::logic_error("Lua runtime session is stopping");
        }
        session->opaqueValues[identity] = &reference;
    };
    if (isEnteredSession(session)) {
        registerValue();
    } else {
        std::scoped_lock luaLock(session->luaMutex);
        registerValue();
    }
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
    if (isEnteredSession(session)) {
        session->opaqueValues.erase(identity);
    } else {
        std::scoped_lock luaLock(session->luaMutex);
        session->opaqueValues.erase(identity);
    }
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
    const auto findValue = [&]() -> LuaRegistryReference {
        if (session->state == nullptr ||
            !sessionAllowsCurrentThread(*session)) {
            return {};
        }
        const auto iterator = session->opaqueValues.find(identity);
        if (iterator == session->opaqueValues.end() ||
            iterator->second == nullptr) {
            return {};
        }
        return *iterator->second;
    };
    if (isEnteredSession(session)) {
        return findValue();
    }
    std::scoped_lock luaLock(session->luaMutex);
    return findValue();
}

void initializeRuntimeSession(lua_State* state) {
    if (state == nullptr) {
        throw std::invalid_argument("Runtime session state cannot be null");
    }
    lua_State* canonicalState = mainThreadFromRegistry(state);
    if (canonicalState == nullptr) {
        throw std::invalid_argument(
            "Runtime session main state is unavailable");
    }
    std::scoped_lock lock(sessionMutex);
    if (activeSession != nullptr) {
        if (activeSession->state == canonicalState &&
            activeSession->phase.load(std::memory_order_acquire) ==
                RuntimeSessionPhase::running) {
            return;
        }
        throw std::logic_error("Only one Lua runtime session may be active");
    }
    activeSession = std::make_shared<RuntimeSessionState>(canonicalState);
}

int enterRuntimeSession(lua_State* state) noexcept {
    return enterSession(state, true);
}

int tryEnterRuntimeSession(lua_State* state) noexcept {
    return enterSession(state, false);
}

void leaveRuntimeSession(lua_State* state) noexcept {
    if (enteredSession.depth == 0 || enteredSession.session == nullptr ||
        (enteredSession.state != state &&
         enteredSession.state != mainThreadFromRegistry(state))) {
        return;
    }
    const std::shared_ptr<RuntimeSessionState> session = enteredSession.session;
    --enteredSession.depth;
    if (enteredSession.depth == 0) {
        enteredSession.state = nullptr;
        enteredSession.session.reset();
        session->luaMutex.unlock();
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
    if (session->state == nullptr ||
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
    lua_State* canonicalState = nullptr;
    {
        std::scoped_lock luaLock(session->luaMutex);
        if (session->state == nullptr ||
            !sessionAllowsCurrentThread(*session)) {
            return;
        }
        canonicalState = session->state;
        cleanups.swap(session->moduleCleanups);
    }
    for (auto cleanup = cleanups.rbegin(); cleanup != cleanups.rend();
         ++cleanup) {
        (*cleanup)(canonicalState);
    }
}

void clearRuntimeRegistryReferences(lua_State* state) noexcept {
    const std::shared_ptr<RuntimeSessionState> session = findSession(state);
    if (session == nullptr) {
        return;
    }
    std::scoped_lock luaLock(session->luaMutex);
    if (session->state == nullptr || !sessionAllowsCurrentThread(*session)) {
        return;
    }
    session->opaqueValues.clear();
    for (RuntimeRegistryReferenceState* reference :
         session->registryReferences) {
        if (reference == nullptr || reference->reference < 0) {
            continue;
        }
        luaL_unref(session->state, LUA_REGISTRYINDEX, reference->reference);
        reference->reference = LUA_NOREF;
    }
    session->registryReferences.clear();
}

void releaseRuntimeSession(lua_State* state) noexcept {
    clearRuntimeRegistryReferences(state);
    const std::shared_ptr<RuntimeSessionState> active = findSession(state);
    if (active == nullptr) {
        return;
    }
    std::shared_ptr<RuntimeSessionState> session;
    {
        std::scoped_lock lock(sessionMutex);
        if (activeSession != active) {
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
