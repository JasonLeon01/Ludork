#pragma once

#include <Runtime/RuntimeValue.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

using NodeIndex = std::variant<int, std::string>;

struct NodeIndexHash {
    std::size_t operator()(const NodeIndex& value) const {
        if (const int* index = std::get_if<int>(&value)) {
            return std::hash<int>{}(*index);
        }
        return std::hash<std::string>{}(std::get<std::string>(value));
    }
};

struct NodeResult {
    RuntimeValue::Array values;
    std::size_t count = 0;
};

using NodeCache = std::unordered_map<NodeIndex, NodeResult, NodeIndexHash>;

struct NodeGraphRuntimeContext {
    RuntimeIdentityPtr localGraph;
    RuntimeValue graph;
};

struct NodeGraphConditionResult {
    NodeResult result;
    bool finished = true;
};
