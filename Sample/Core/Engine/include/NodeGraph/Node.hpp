#pragma once

#include <BindAnnotations.hpp>
#include <NodeGraph/Types.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Graph;

struct NodeNamedValues {
    std::string name;
    RuntimeValue::Array values;
};

struct NodeMemberMetadata {
    std::vector<std::string> parameterOrder;
    RuntimeValue::Map parameterTypes;
    RuntimeValue::Map parameterDefaults;
    std::vector<NodeNamedValues> execSplits;
    std::vector<NodeNamedValues> latentStates;
    bool latent = false;
    bool loop = false;
    bool pure = false;
    std::string loopNode;
    std::string kind;
};

BIND_CLASS(bind_bases = false, cast_bases = "RuntimeObject", metadata = false)
class DataNode : public RuntimeObject {
public:
    BIND_INIT()
    DataNode(std::string nodeFunction, RuntimeValue params,
             RuntimeValue resolvedDefinition);

    ~DataNode() override = default;

    BIND_PROPERTY(metadata = false)
    std::string nodeFunction;

    BIND_PROPERTY(metadata = false)
    RuntimeValue params;

    BIND_PROPERTY(metadata = false)
    RuntimeValue position;

    RuntimeValue::Array getParams() const;

    const RuntimeValue& getResolvedDefinition() const;

private:
    RuntimeValue resolvedDefinition_;
};

BIND_CLASS(bind_bases = false, cast_bases = "RuntimeObject", metadata = false)
class Node : public RuntimeObject {
public:
    using InputPinMap = std::unordered_map<int, RuntimeValue>;

    BIND_INIT(allow_nil = "parent")
    Node(std::shared_ptr<Graph> parentGraph, RuntimeValue parent,
         std::string functionName, RuntimeIdentityPtr nodeFunction,
         RuntimeValue params);

    ~Node() override = default;

    BIND_METHOD()
    RuntimeValue::Map getParamList() const;

    BIND_METHOD()
    RuntimeValue::Map getParamDefaults() const;

    BIND_METHOD(defaults = {nil})
    RuntimeValue::Array execute(const InputPinMap& inputPinReplace = {});

    BIND_METHOD()
    RuntimeValue::Map asDict() const;

    BIND_METHOD(name = "__repr__", metadata = false)
    std::string repr() const;

    BIND_METHOD(name = "__tostring", metadata = false)
    std::string toString() const;

    BIND_METHOD()
    static RuntimeIdentityPtr getRefLocal(
        const RuntimeIdentityPtr& nodeFunction);

    BIND_PROPERTY(metadata = false)
    std::string functionName;

    BIND_PROPERTY(metadata = false)
    RuntimeValue params;

    BIND_PROPERTY(metadata = false)
    RuntimeValue position;

    NodeResult executeResult(const InputPinMap& inputPinReplace = {});
    const NodeMemberMetadata& getMemberMetadata() const;
    const RuntimeIdentityPtr& getCallable() const;
    std::shared_ptr<Graph> getParentGraph() const;

private:
    friend class Graph;

    struct ResolvedCallable {
        RuntimeIdentityPtr callable;
        RuntimeValue::Map descriptor;
        NodeMemberMetadata metadata;
        std::vector<std::string> parameterNames;
        std::string declaringModule;
        bool selfFunction = false;
        std::string displayName;
    };

    static ResolvedCallable resolvedCallable(
        const RuntimeValue& resolvedDefinition);
    static NodeMemberMetadata parseMemberMetadata(
        const RuntimeValue& metadataValue);
    static NodeResult parseInvocationResult(
        const std::vector<RuntimeValue>& resolvedValues);

    Node(Graph& parentGraph, RuntimeValue parent, std::string functionName,
         RuntimeValue resolvedDefinition, RuntimeValue params);
    Node(Graph& parentGraph, RuntimeValue parent,
         std::shared_ptr<const Node> definition);
    void initialise(RuntimeValue params, RuntimeValue resolvedDefinition);
    void attachParentGraph(const std::shared_ptr<Graph>& parentGraph);
    void analyseFunction(const RuntimeValue& resolvedDefinition);
    RuntimeValue getParent() const;
    RuntimeValue::Array resolveStoredParams(
        const RuntimeValue::Array& rawParams);
    const Node& compiledDefinition() const;

    std::weak_ptr<Graph> parentGraph_;
    std::shared_ptr<const Node> definition_;
    RuntimeIdentityPtr nodeFunction_;
    RuntimeValue::Map paramList_;
    RuntimeValue::Map paramDefaults_;
    NodeMemberMetadata memberMetadata_;
    std::vector<std::string> paramOrder_;
    std::string declaringModule_;
    std::string funcInfo_;
    bool selfFunction_ = false;
    std::size_t paramCount_ = 0;
};
