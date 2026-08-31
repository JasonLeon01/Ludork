#pragma once

#include <BindAnnotations.hpp>
#include <NodeGraph/Node.hpp>
#include <NodeGraph/Types.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ludork::engine::graph_detail {
struct ExecutionState;
struct LoopResult;
}  // namespace ludork::engine::graph_detail

BIND_CLASS(copyable = true, table_init = true, metadata = false)
struct GraphLink {
    BIND_PROPERTY(metadata = false)
    RuntimeValue left;

    BIND_PROPERTY(metadata = false)
    RuntimeValue right;

    BIND_PROPERTY(metadata = false)
    int leftOutPin = 0;

    BIND_PROPERTY(metadata = false)
    int rightInPin = 0;

    BIND_PROPERTY(metadata = false)
    std::string linkType;
};

BIND_CLASS(bind_bases = false, cast_bases = {"RuntimeObject"}, metadata = false)
class Graph : public RuntimeObject {
public:
    using DataNodeMap =
        std::unordered_map<std::string, std::vector<std::shared_ptr<DataNode>>>;
    using LinkMap = std::unordered_map<std::string, std::vector<GraphLink>>;
    using EventParams =
        std::unordered_map<std::string, std::vector<std::string>>;

    struct NodeSource {
        NodeIndex node;
        int pin = 0;
    };

    using PinSources = std::unordered_map<int, NodeSource>;
    using EventRely = std::unordered_map<int, PinSources>;
    using RelyMap = std::unordered_map<std::string, EventRely>;
    using PinNexts = std::unordered_map<int, NodeSource>;
    using EventNexts = std::unordered_map<int, PinNexts>;
    using NextMap = std::unordered_map<std::string, EventNexts>;

    BIND_INIT(defaults = {nil, nil, nil}, allow_nil = "parentClass,parent")
    Graph(std::string parentClassName, RuntimeValue parentClass,
          RuntimeValue parent, DataNodeMap inNodes, LinkMap links,
          RuntimeValue nodeModel = {}, RuntimeValue startNodeValues = {},
          EventParams eventParams = {});

    Graph(const Graph& other);
    Graph& operator=(const Graph& other);
    ~Graph() override;

    BIND_METHOD()
    void genNodesFromDataNodes();

    BIND_METHOD()
    void genRelationsFromLinks();

    BIND_METHOD(metadata = false, allow_nil = "parentValue")
    std::shared_ptr<Graph> instantiate(RuntimeValue parentValue);

    BIND_METHOD(defaults = {nil, nil, nil})
    RuntimeValue::Array execute(
        const std::string& key, std::optional<int> startNode = std::nullopt,
        std::optional<std::size_t> limit = std::nullopt,
        std::optional<RuntimeIdentityPtr> cache = std::nullopt);

    BIND_METHOD(defaults = {nil})
    RuntimeValue::Array executeNode(
        const std::string& key, const NodeIndex& nodeIndex,
        std::optional<RuntimeIdentityPtr> cache = std::nullopt);

    BIND_METHOD()
    std::vector<NodeIndex> getRelyNodeIndexList(
        const std::string& key, const NodeIndex& nodeIndex) const;

    BIND_METHOD()
    std::vector<std::shared_ptr<Node>> getNodes(const std::string& key) const;

    BIND_METHOD()
    RuntimeIdentityPtr getFunctionFromModule(const RuntimeValue& module,
                                             const std::string& path) const;

    BIND_METHOD()
    RuntimeIdentityPtr getFunctionFromObject(const RuntimeValue& object,
                                             const std::string& path) const;

    BIND_METHOD()
    bool hasKey(const std::string& key) const;

    BIND_METHOD(Pure = true)
    bool hasExecutableEvent(const std::string& key) const;

    BIND_METHOD()
    bool tryLockExecution(const std::string& key);

    BIND_METHOD()
    bool isExecutionLocked(const std::string& key) const;

    BIND_METHOD()
    void onLatentAdded(const std::string& key);

    BIND_METHOD()
    void onLatentResolved(const std::string& key);

    BIND_METHOD()
    std::size_t getLatentPendingCount(const std::string& key) const;

    BIND_METHOD()
    void addExecutionCompleteCallback(const std::string& key,
                                      std::function<void()> callback);

    BIND_METHOD()
    void resumeSuspendedLoops(const std::string& key);

    BIND_METHOD()
    void completeExecution(const std::string& key);

    BIND_METHOD()
    RuntimeValue::Map asDict() const;

    BIND_PROPERTY(metadata = false)
    std::string parentClassName;

    BIND_PROPERTY(metadata = false)
    RuntimeValue parentClass;

    BIND_METHOD(property = "parent", setter = "setParent", metadata = false)
    RuntimeValue getParent() const;

    void setParent(RuntimeValue value);

    BIND_PROPERTY(metadata = false)
    RuntimeIdentityPtr localGraph;

    BIND_PROPERTY(metadata = false)
    std::unordered_map<std::string, int> startNodes;

    NodeResult executeResult(const std::string& key,
                             std::optional<int> startNode = std::nullopt,
                             std::size_t limit = 1000000,
                             NodeCache* cache = nullptr);
    NodeResult executeNodeResult(const std::string& key,
                                 const NodeIndex& nodeIndex, NodeCache& cache);
    const PinNexts& getNodeNexts(const std::string& key, int nodeIndex) const;
    const RuntimeIdentityPtr& getLocalGraph() const;
    void setLocalGraph(RuntimeIdentityPtr context);
    const RuntimeValue& getGraphContext() const;
    const std::string& getDoingPartKey() const;

private:
    struct InstanceTag {};

    void ensureInitialised();
    void ensureEventInitialised(const std::string& key);
    void initializeContext(RuntimeValue parentValue);
    Graph(const Graph& definition, RuntimeValue parentValue, InstanceTag);
    void cloneEventNodesFrom(const Graph& definition, const std::string& key,
                             const std::shared_ptr<Graph>& self);
    void buildNodesFromDataNodes();
    void buildNodesForEvent(const std::string& key);
    const DataNodeMap& dataNodes() const;
    const LinkMap& links() const;
    const EventParams& eventParams() const;
    const RelyMap& nodeRely() const;
    const NextMap& nodeNexts() const;
    ludork::engine::graph_detail::LoopResult executeLoopNode(
        const std::string& key, int nodeIndex, const NodeResult& controlResult,
        NodeCache& cache, std::size_t limit);
    std::optional<int> getNamedExecPinIndex(const NodeMemberMetadata& metadata,
                                            const std::string& pinName) const;
    NodeResult getLoopEmptyResult(const NodeMemberMetadata& metadata) const;
    std::vector<NodeResult> iterateLoopResults(
        const NodeMemberMetadata& metadata,
        const NodeResult& controlResult) const;
    bool runLoopBodyIteration(const std::string& key, int loopNodeIndex,
                              int bodyStart,
                              const std::vector<NodeIndex>& bodyCacheKeys,
                              const NodeCache& baseCache,
                              const NodeResult& loopResult, std::size_t limit);
    std::vector<NodeIndex> getLoopBodyCacheKeys(const std::string& key,
                                                int loopNodeIndex,
                                                int startNode) const;
    RuntimeValue contextValue(const std::string& name) const;

    DataNodeMap dataNodes_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<Node>>> nodes_;
    std::shared_ptr<Graph> definition_;
    LinkMap links_;
    RuntimeValue nodeModel_;
    EventParams eventParams_;
    RuntimeValue graphContext_;
    RelyMap nodeRely_;
    NextMap nodeNexts_;
    std::unique_ptr<ludork::engine::graph_detail::ExecutionState>
        executionState_;
    bool initialised_ = false;
    bool initialising_ = false;
    std::unordered_set<std::string> initialisedEvents_;
};
