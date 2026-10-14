#include <Graphics/Materials/MaterialDefinition.hpp>
#include "MaterialSchema.hpp"

#include <Runtime/RuntimeDataReader.hpp>

#include <stdexcept>
#include <unordered_set>

namespace ludork::engine::materials {

std::shared_ptr<const MaterialDefinition> MaterialDefinition::fromData(
    const RuntimeData& data) {
    const RuntimeData::Map& root =
        ludork::runtime::value_reader::requireMap(data, "material");
    const RuntimeData::Map& graph = ludork::runtime::value_reader::requireMap(
        ludork::runtime::value_reader::requireValue(root, "graph", "material"),
        "material.graph");
    std::shared_ptr<MaterialDefinition> result =
        std::make_shared<MaterialDefinition>();
    result->data_ = data;
    result->output_ = ludork::runtime::value_reader::requireString(
        ludork::runtime::value_reader::requireValue(graph, "output",
                                                    "material.graph"),
        "material.graph.output");
    std::unordered_set<std::string> nodeIds;
    std::unordered_set<std::string> nodeTypes;
    for (const RuntimeData& definition : schema::nodeDefinitions()) {
        nodeTypes.insert(*definition.getIf<RuntimeData::Map>()
                              ->at("type")
                              .getIf<std::string>());
    }
    for (const RuntimeData& entry : ludork::runtime::value_reader::requireArray(
             ludork::runtime::value_reader::requireValue(graph, "nodes",
                                                         "material.graph"),
             "material.graph.nodes")) {
        const RuntimeData::Map& fields =
            ludork::runtime::value_reader::requireMap(entry, "material node");
        Node node;
        node.id = ludork::runtime::value_reader::requireString(
            ludork::runtime::value_reader::requireValue(fields, "id",
                                                        "material node"),
            "material node.id");
        node.type = ludork::runtime::value_reader::requireString(
            ludork::runtime::value_reader::requireValue(fields, "type",
                                                        "material node"),
            "material node.type");
        if (node.id.empty() || !nodeIds.insert(node.id).second) {
            throw std::invalid_argument(
                "Material node ID must be non-empty and unique: " + node.id);
        }
        if (!nodeTypes.contains(node.type)) {
            throw std::invalid_argument("Unknown material node type: " +
                                        node.type);
        }
        if (const RuntimeData* inputs =
                ludork::runtime::value_reader::findValue(fields, "inputs")) {
            node.inputs = ludork::runtime::value_reader::requireMap(
                *inputs, "material node.inputs");
        }
        if (const RuntimeData* properties =
                ludork::runtime::value_reader::findValue(fields,
                                                         "properties")) {
            node.properties = ludork::runtime::value_reader::requireMap(
                *properties, "material node.properties");
        }
        result->nodes_.push_back(std::move(node));
    }
    return result;
}

const RuntimeData& MaterialDefinition::data() const {
    return data_;
}

const std::vector<MaterialDefinition::Node>& MaterialDefinition::nodes() const {
    return nodes_;
}

const std::string& MaterialDefinition::output() const {
    return output_;
}

}  // namespace ludork::engine::materials
