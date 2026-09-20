#pragma once

#include <Runtime/RuntimeObject.hpp>

#include <CoreMinimal.hpp>
#include <RuntimeApi.hpp>

#include <Runtime/NodeGraph/Types.hpp>
#include <Runtime/NodeGraph/NodeDefinition.hpp>

class Graph;

BIND_CLASS(bind_bases = false, cast_bases = {"RuntimeObject"}, metadata = false)
class LUDORK_RUNTIME_API Node : public RuntimeObject {
public:
    LUDORK_CAST_DERIVED(Node, RuntimeObject)

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

    Node(Graph& parentGraph, RuntimeValue parent, std::string functionName,
         std::shared_ptr<const NodeDefinition> definition, RuntimeValue params);
    Node(Graph& parentGraph, RuntimeValue parent,
         std::shared_ptr<const Node> definition);
    void initialise(RuntimeValue params);
    void attachParentGraph(const std::shared_ptr<Graph>& parentGraph);
    RuntimeValue getParent() const;
    RuntimeValue::Array resolveStoredParams(
        const RuntimeValue::Array& rawParams);

    std::weak_ptr<Graph> parentGraph_;
    std::shared_ptr<const NodeDefinition> definition_;
    std::string funcInfo_;
    std::size_t paramCount_ = 0;
};
