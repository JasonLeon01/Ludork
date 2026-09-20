#include "NodeGraphRuntimeInternal.hpp"
#include "StackRestore.hpp"
#include "LuaServices/RuntimeBindingTraits.hpp"
#include "LuaServices/RuntimeReferenceConversion.hpp"
#include <Runtime/NodeGraph/Graph.hpp>
#include <Runtime/NodeGraph/Node.hpp>
#include <Runtime/Detail/RuntimeServices.hpp>
#include <LudorkRuntimeBinding/DynamicValueCodec.hpp>
#include <Runtime/RuntimeObject.hpp>

#include <climits>
#include <exception>
#include <limits>
#include <stdexcept>

namespace ludork::runtime::node_graph_detail {

StackRestore::~StackRestore() {
    lua_settop(state, top);
}

namespace {
constexpr const char* NODEGRAPH_REF_LOCALS_KEY =
    "Ludork.Runtime.NodeGraph.refLocals";
constexpr const char* NODEGRAPH_CONTEXTS_KEY =
    "Ludork.Runtime.NodeGraph.contexts";

lua_glue::Object write(lua_glue::StateView lua, const RuntimeValue& value) {
    return binding::writeLuaValue(lua, value);
}
RuntimeValue snapshot(const lua_glue::Object& value) {
    return binding::readLuaValue<RuntimeValue>(value);
}
RuntimeHandle capture(const lua_glue::Object& value) {
    return RuntimeHandle(
        binding::readOpaqueIdentity<RuntimeIdentityPtr>(value));
}
lua_glue::Table requireTable(const lua_glue::Object& value) {
    if (!value.is<lua_glue::Table>()) {
        throw std::invalid_argument("Runtime value must be a table");
    }
    return value.as<lua_glue::Table>();
}
bool isCallable(lua_glue::StateView lua, const lua_glue::Object& value) {
    if (value.get_type() == lua_glue::Type::Function) {
        return true;
    }
    if (value.get_type() != lua_glue::Type::Table &&
        value.get_type() != lua_glue::Type::Userdata) {
        return false;
    }
    return detail::objectMetatable(lua, value)
               .raw_get<lua_glue::Object>("__call")
               .get_type() == lua_glue::Type::Function;
}
std::size_t packedCount(const lua_glue::Table& values) {
    std::int64_t count = 0;
    return binding::luaIntegerValue(values.raw_get<lua_glue::Object>("n"),
                                    count) &&
                   count >= 0
               ? static_cast<std::size_t>(count)
               : values.size();
}
NodeResult collect(lua_State* state, int base, int count) {
    NodeResult result;
    result.count = static_cast<std::size_t>(count);
    result.values.reserve(result.count);
    for (int index = 1; index <= count; ++index) {
        result.values.push_back(
            snapshot(lua_glue::Read<lua_glue::Object>(state, base + index)));
    }
    return result;
}
NodeResult readPacked(const lua_glue::Object& value) {
    NodeResult result;
    if (!value.is<lua_glue::Table>()) {
        return {{snapshot(value)}, 1};
    }
    const lua_glue::Table table = value.as<lua_glue::Table>();
    result.count = packedCount(table);
    if (result.count > static_cast<std::size_t>(INT_MAX)) {
        throw std::length_error("Node graph cache value count overflow");
    }
    result.values.reserve(result.count);
    for (std::size_t index = 1; index <= result.count; ++index) {
        result.values.push_back(
            snapshot(table.raw_get<lua_glue::Object>(index)));
    }
    return result;
}
lua_glue::Table contextTarget(const lua_glue::Object& value) {
    const lua_glue::Table table = requireTable(value);
    const lua_glue::Object graph = table.raw_get<lua_glue::Object>("__graph__");
    return graph.is<lua_glue::Table>() ? graph.as<lua_glue::Table>() : table;
}
NodeIndex cacheKey(const lua_glue::Object& value) {
    if (value.get_type() == lua_glue::Type::String) {
        return value.as<std::string>();
    }
    std::int64_t index = 0;
    if (!binding::luaIntegerValue(value, index)) {
        throw std::invalid_argument(
            "Node graph cache keys must be strings or integers");
    }
    if (index < INT_MIN || index > INT_MAX) {
        throw std::out_of_range("Node graph cache integer key is out of range");
    }
    return static_cast<int>(index);
}
}  // namespace

NodeGraphContextObjects createNodeGraphContext(
    RuntimeScope& scope, const RuntimeValue& parentClass,
    const RuntimeValue& parentValue) {
    lua_glue::StateView lua(scope.state());
    lua_glue::Table context = lua.create_table();
    lua_glue::SetMetatable(context, lua.create_table());
    lua_glue::Table graph = lua.create_table();
    graph.raw_set("parentClass", write(lua, parentClass));
    graph.raw_set("localGraph", context);
    lua_glue::Table parent = detail::createWeakTable(lua, "v");
    parent.raw_set(1, write(lua, parentValue));
    lua_glue::Table meta = lua.create_table();
    meta.set_function(
        "__index",
        [parent](const lua_glue::Object&,
                 const lua_glue::Object& key) -> lua_glue::Object {
            return key.get_type() == lua_glue::Type::String &&
                           key.as<std::string>() == "parent"
                       ? parent.raw_get<lua_glue::Object>(1)
                       : detail::nilObject(
                             lua_glue::StateView(key.lua_state()));
        });
    meta.set_function(
        "__newindex",
        [parent](lua_glue::Table target, const lua_glue::Object& key,
                 const lua_glue::Object& value) mutable {
            if (key.get_type() == lua_glue::Type::String &&
                key.as<std::string>() == "parent") {
                parent.raw_set(1, value);
            } else {
                target.raw_set(key, value);
            }
        });
    lua_glue::SetMetatable(graph, meta);
    context.raw_set("__graph__", graph);
    detail::registryTable(lua, NODEGRAPH_CONTEXTS_KEY, "k")
        .raw_set(graph, context);
    return {capture(lua_glue::MakeObject(lua, context)),
            capture(lua_glue::MakeObject(lua, graph))};
}
RuntimeValue getNodeGraphContextValue(RuntimeScope& scope,
                                      const RuntimeHandle& context,
                                      const std::string& key,
                                      NodeGraphValueRead mode) {
    const lua_glue::Object value =
        requireTable(write(lua_glue::StateView(scope.state()), context))
            .raw_get<lua_glue::Object>(key);
    return mode == NodeGraphValueRead::Snapshot
               ? snapshot(value)
               : detail::readRuntimeReference(value);
}
void setNodeGraphContextValue(RuntimeScope& scope, const RuntimeHandle& context,
                              const std::string& key,
                              const RuntimeValue& value) {
    lua_glue::StateView lua(scope.state());
    requireTable(write(lua, context)).raw_set(key, write(lua, value));
}
RuntimeValue getNodeGraphContextParent(RuntimeScope& scope,
                                       const RuntimeValue& graph) {
    lua_glue::StateView lua(scope.state());
    return snapshot(detail::runtimeIndex(
        lua, lua_glue::MakeObject(lua, contextTarget(write(lua, graph))),
        lua_glue::MakeObject(lua, "parent"), false));
}
void setNodeGraphContextParent(RuntimeScope& scope, const RuntimeValue& graph,
                               const RuntimeValue& parent) {
    lua_glue::StateView lua(scope.state());
    detail::runtimeAssign(
        lua, lua_glue::MakeObject(lua, contextTarget(write(lua, graph))),
        lua_glue::MakeObject(lua, "parent"), write(lua, parent), false);
}
std::shared_ptr<Node> createNodeGraphNode(
    RuntimeScope& scope, const RuntimeValue& nodeModel,
    const std::shared_ptr<Graph>& graph, const RuntimeValue& parent,
    const std::string& nodeFunction, const RuntimeHandle& fallback,
    const RuntimeValue::Array& parameters) {
    if (nodeModel.isNil()) {
        return nullptr;
    }
    lua_glue::StateView lua(scope.state());
    const lua_glue::Object constructor = detail::runtimeIndex(
        lua, write(lua, nodeModel), lua_glue::MakeObject(lua, "new"), false);
    if (constructor.get_type() != lua_glue::Type::Function) {
        return nullptr;
    }
    const int base = lua_gettop(scope.state());
    StackRestore restore{scope.state(), base};
    const int count = detail::invokeRuntimeFunction(
        scope.state(), constructor,
        {write(lua, RuntimeValue(graph)), write(lua, parent),
         lua_glue::MakeObject(lua, nodeFunction), write(lua, fallback),
         binding::writeLuaValue(lua, parameters)},
        "node constructor arguments");
    if (count == 0 || lua_isnil(scope.state(), base + 1)) {
        return nullptr;
    }
    std::shared_ptr<RuntimeObject> object;
    binding::tryReadSharedPointer(
        lua_glue::Read<lua_glue::Object>(scope.state(), base + 1), object);
    const auto node = ludork::Cast<Node>(object);
    if (node == nullptr) {
        throw std::runtime_error(
            "Node model constructor must return an Engine.Node or nil");
    }
    return node;
}
NodeResult invokeNodeGraphCallable(RuntimeScope& scope,
                                   const RuntimeHandle& callable,
                                   const RuntimeValue& self,
                                   std::span<const RuntimeValue> arguments,
                                   const RuntimeHandle& context) {
    lua_glue::StateView lua(scope.state());
    const lua_glue::Object function = write(lua, callable);
    const lua_glue::Object receiver = write(lua, self);
    const lua_glue::Object local = write(lua, context);
    if (!isCallable(lua, function)) {
        throw std::runtime_error("Node graph value is not callable");
    }
    const std::size_t selfCount = binding::isNil(receiver) ? 0 : 1;
    if (arguments.size() > static_cast<std::size_t>(INT_MAX) - selfCount) {
        throw std::length_error("Node graph argument count overflow");
    }
    std::vector<lua_glue::Object> values;
    values.reserve(arguments.size() + selfCount);
    if (selfCount) {
        values.push_back(receiver);
    }
    for (const RuntimeValue& value : arguments) {
        values.push_back(write(lua, value));
    }
    lua_glue::Table refLocals =
        detail::registryTable(lua, NODEGRAPH_REF_LOCALS_KEY, "k");
    const lua_glue::Object oldRef =
        refLocals.raw_get<lua_glue::Object>(function);
    lua_glue::Object oldActive = detail::nilObject(lua);
    const bool hasContext = local.is<lua_glue::Table>();
    if (hasContext) {
        lua_glue::Table table = local.as<lua_glue::Table>();
        oldActive = table.raw_get<lua_glue::Object>("__activeNodeFunction__");
        table.raw_set("__activeNodeFunction__", function);
        refLocals.raw_set(function, local);
    }
    const int base = lua_gettop(scope.state());
    StackRestore restore{scope.state(), base};
    int count = 0;
    std::exception_ptr failure;
    try {
        count = detail::invokeRuntimeFunction(scope.state(), function, values,
                                              "node graph arguments");
    } catch (...) {
        failure = std::current_exception();
    }
    if (hasContext) {
        local.as<lua_glue::Table>().raw_set("__activeNodeFunction__",
                                            oldActive);
        refLocals.raw_set(function, oldRef);
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
    return collect(scope.state(), base, count);
}
RuntimeHandle nodeGraphRefLocal(RuntimeScope& scope,
                                const RuntimeHandle& callable) {
    lua_glue::StateView lua(scope.state());
    return capture(detail::registryTable(lua, NODEGRAPH_REF_LOCALS_KEY, "k")
                       .raw_get<lua_glue::Object>(write(lua, callable)));
}
NodeGraphConditionResult evaluateNodeGraphCondition(
    RuntimeScope& scope, const RuntimeHandle& condition) {
    lua_glue::StateView lua(scope.state());
    const lua_glue::Object callable = write(lua, condition);
    if (!isCallable(lua, callable)) {
        throw std::invalid_argument("Runtime value is not callable");
    }
    const int base = lua_gettop(scope.state());
    StackRestore restore{scope.state(), base};
    const int count = detail::invokeRuntimeFunction(scope.state(), callable, {},
                                                    "node graph condition");
    NodeGraphConditionResult result;
    if (count != 0 && !lua_isnil(scope.state(), base + 1)) {
        const lua_glue::Object first =
            lua_glue::Read<lua_glue::Object>(scope.state(), base + 1);
        if (first.is<lua_glue::Table>()) {
            const lua_glue::Table table = first.as<lua_glue::Table>();
            std::int64_t length = 0;
            if ((binding::luaIntegerValue(table.raw_get<lua_glue::Object>("n"),
                                          length) &&
                 length >= 0) ||
                table.size() > 0) {
                result.result = readPacked(first);
            } else {
                for (const auto& entry : table) {
                    result.result.values.push_back(snapshot(entry.second));
                }
                result.result.count = result.result.values.size();
            }
        } else {
            result.result = {{snapshot(first)}, 1};
        }
    }
    lua_settop(scope.state(), base);
    if (callable.is<lua_glue::Table>()) {
        const lua_glue::Object finished = detail::runtimeIndex(
            lua, callable, lua_glue::MakeObject(lua, "isFinished"), false);
        if (isCallable(lua, finished)) {
            const int finishedCount = detail::invokeRuntimeFunction(
                scope.state(), finished, {callable},
                "node graph condition isFinished");
            if (finishedCount != 1 ||
                lua_type(scope.state(), base + 1) != LUA_TBOOLEAN) {
                throw std::runtime_error(
                    "Node graph condition isFinished must return exactly one "
                    "boolean");
            }
            result.finished = lua_toboolean(scope.state(), base + 1) != 0;
        }
    }
    return result;
}
NodeCache readNodeGraphCache(RuntimeScope& scope, const RuntimeHandle& cache) {
    lua_glue::StateView lua(scope.state());
    const lua_glue::Object table = write(lua, cache);
    NodeCache result;
    if (!table.is<lua_glue::Table>()) {
        return result;
    }
    for (const auto& entry : table.as<lua_glue::Table>()) {
        result.emplace(cacheKey(entry.first), readPacked(entry.second));
    }
    return result;
}
void writeNodeGraphCache(RuntimeScope& scope, const RuntimeHandle& cache,
                         const NodeCache& values) {
    lua_glue::StateView lua(scope.state());
    const lua_glue::Object value = write(lua, cache);
    if (!value.is<lua_glue::Table>()) {
        return;
    }
    lua_glue::Table table = value.as<lua_glue::Table>();
    std::vector<lua_glue::Object> keys;
    for (const auto& entry : table) {
        keys.push_back(entry.first);
    }
    for (const lua_glue::Object& key : keys) {
        table.raw_set(key, lua_glue::nil);
    }
    for (const auto& [key, result] : values) {
        if (result.count > static_cast<std::size_t>(INT_MAX)) {
            throw std::length_error("Node graph cache value count overflow");
        }
        lua_glue::Table packed =
            lua.create_table(static_cast<int>(result.count), 1);
        packed.raw_set("n", result.count);
        for (std::size_t index = 0;
             index < result.count && index < result.values.size(); ++index) {
            packed.raw_set(index + 1, write(lua, result.values[index]));
        }
        std::visit(
            [&](const auto& index) {
                table.raw_set(index, packed);
            },
            key);
    }
}
void clearNodeGraphRuntimeCaches(lua_State* state) noexcept {
    lua_glue::StateView lua(state);
    lua.registry().raw_set(NODEGRAPH_REF_LOCALS_KEY, lua_glue::nil);
    lua.registry().raw_set(NODEGRAPH_CONTEXTS_KEY, lua_glue::nil);
}
}  // namespace ludork::runtime::node_graph_detail
