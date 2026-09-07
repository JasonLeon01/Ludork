#pragma once

#include <Runtime/ScriptStore.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ludork::runtime {

class HotReloadImpl final {
public:
    explicit HotReloadImpl(lua_State* state);
    ~HotReloadImpl();
    void run();

private:
    struct Pair {
        int live;
        int candidate;
        std::string path;
    };
    struct Upvalue {
        int liveFunction;
        int candidateFunction;
        int liveIndex;
        int candidateIndex;
    };
    struct Environment {
        int liveFunction;
        int liveIndex;
        int candidateFunction;
        int candidateIndex;
        int candidateValue;
        std::string path;
    };
    struct Write {
        enum class Kind {
            Table,
            Upvalue,
            Uservalue
        };
        Kind kind;
        int owner;
        int key;
        int value;
        int index;
    };

    int keep(int index);
    void push(int reference) const;
    int getField(int reference, const char* name);
    int getType(int reference) const;
    const void* pointer(int reference) const;
    bool equal(int left, int right) const;
    std::string stringValue(int reference) const;
    std::vector<std::pair<int, int>> entries(int reference);
    bool isClass(int reference) const;
    bool isLuaFunction(int reference) const;
    bool isClassInternal(int key) const;
    void fail(const std::string& path, const std::string& reason) const;

    void compile();
    void loadCandidates();
    int loadModule(const std::string& name);
    int execute(const std::string& path, const std::string& name);
    void preparePairs();
    void pairValue(int live, int candidate, const std::string& path);
    void pairTable(const Pair& pair);
    void pairFunction(const Pair& pair);
    void prepareEnvironments();
    void prepareReferences();
    void visit(int reference);
    int replacement(int reference) const;
    void commit();
    static int requireCandidate(lua_State* state);

    lua_State* state_;
    int stackBase_;
    int references_ = LUA_NOREF;
    int nextReference_ = 0;
    const void* referencesPointer_{};
    ScriptStore::ReloadSnapshot snapshot_;
    std::string phase_ = "scan";
    int globals_{};
    int environment_{};
    int package_{};
    int loaded_{};
    int candidateToLive_{};
    int mixins_{};
    std::map<std::string, int> chunks_;
    std::map<std::string, int> modules_;
    std::map<std::string, int> liveModules_;
    std::unordered_set<std::string> loading_;
    std::vector<Pair> roots_;
    std::vector<Pair> tables_;
    std::vector<Pair> functions_;
    std::vector<Pair> classes_;
    std::vector<Pair> mixinClasses_;
    std::vector<Upvalue> upvalues_;
    std::vector<Environment> environments_;
    std::unordered_map<const void*, int> objects_;
    std::unordered_map<const void*, int> functionsByLive_;
    std::unordered_map<const void*, const void*> liveCells_;
    std::unordered_map<const void*, const void*> candidateCells_;
    std::unordered_set<const void*> excluded_;
    std::unordered_set<const void*> visited_;
    std::vector<int> pending_;
    std::vector<Write> writes_;
    std::size_t moduleCount_{};
    std::size_t mixinCount_{};
};

}  // namespace ludork::runtime
