#pragma once

#include "Internal.hpp"

#include <NodeGraph/Types.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace ludork::engine::graph_detail {

template <typename LinkMap, typename RelyMap, typename NextMap>
void buildRelations(const LinkMap& links, RelyMap& relies, NextMap& nexts) {
    relies.clear();
    nexts.clear();
    for (const auto& [key, eventLinks] : links) {
        auto& eventRely = relies[key];
        auto& eventNexts = nexts[key];
        for (const auto& link : eventLinks) {
            const std::optional<NodeIndex> left = nodeIndexValue(link.left);
            const std::optional<NodeIndex> right = nodeIndexValue(link.right);
            if (!left.has_value() || !right.has_value()) {
                continue;
            }
            if (link.linkType == "Params") {
                if (const int* rightIndex = std::get_if<int>(&*right)) {
                    eventRely[*rightIndex][link.rightInPin] =
                        typename RelyMap::mapped_type::mapped_type::mapped_type{
                            *left, link.leftOutPin};
                }
            } else if (link.linkType == "Exec") {
                if (const int* leftIndex = std::get_if<int>(&*left)) {
                    eventNexts[*leftIndex][link.leftOutPin] =
                        typename NextMap::mapped_type::mapped_type::mapped_type{
                            *right, link.rightInPin};
                }
            }
        }
    }
}

template <typename RelyMap>
std::vector<NodeIndex> dependencyOrder(const RelyMap& relies,
                                       const std::string& key,
                                       const NodeIndex& nodeIndex) {
    std::vector<NodeIndex> order;
    std::set<NodeIndex> visited;
    const auto event = relies.find(key);
    if (event == relies.end()) {
        return order;
    }
    const auto visit = [&](const auto& self, const NodeIndex& current) -> void {
        if (!visited.insert(current).second) {
            return;
        }
        if (const std::string* name = std::get_if<std::string>(&current);
            name != nullptr && name->rfind("default_", 0) == 0) {
            return;
        }
        const int* currentIndex = std::get_if<int>(&current);
        if (currentIndex == nullptr) {
            return;
        }
        const auto dependencies = event->second.find(*currentIndex);
        if (dependencies == event->second.end()) {
            return;
        }
        for (const int inputPin : sortedPins(dependencies->second)) {
            const NodeIndex& left = dependencies->second.at(inputPin).node;
            self(self, left);
            if (std::find(order.begin(), order.end(), left) == order.end()) {
                order.push_back(left);
            }
        }
    };
    visit(visit, nodeIndex);
    return order;
}

}  // namespace ludork::engine::graph_detail
