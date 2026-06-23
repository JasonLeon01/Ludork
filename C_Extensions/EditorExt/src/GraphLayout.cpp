#include <GraphLayout.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>


namespace {

struct GraphLayoutOptions {
    double xStep = 720.0;
    double yStep = 320.0;
    double defaultParamYStep = 64.0;
    double defaultParamStartY = 64.0;
    double startGap = 250.0;
    double columnPadding = 24.0;
};

using Position = std::pair<double, double>;

class PositionStore {
public:
    void setDefault(int idx, double x, double y) { defaults_[idx] = {x, y}; }

    void setNode(int idx, double x, double y) { nodes_[idx] = {x, y}; }

    bool hasNode(int idx) const { return nodes_.find(idx) != nodes_.end(); }

    Position &node(int idx) { return nodes_.at(idx); }

    const Position &node(int idx) const { return nodes_.at(idx); }

    std::vector<int> nodeKeys() const {
        std::vector<int> keys;
        keys.reserve(nodes_.size());
        for (const auto &entry : nodes_) {
            keys.push_back(entry.first);
        }
        return keys;
    }

    std::vector<int> sortedNodeKeysByPos() const {
        auto keys = nodeKeys();
        std::sort(keys.begin(), keys.end(), [this](int left, int right) {
            const Position &leftPos = nodes_.at(left);
            const Position &rightPos = nodes_.at(right);
            if (leftPos.first != rightPos.first) {
                return leftPos.first < rightPos.first;
            }
            return leftPos.second < rightPos.second;
        });
        return keys;
    }

    py::dict toDict() const {
        py::dict result;
        for (const auto &entry : defaults_) {
            result[py::str("default_" + std::to_string(entry.first))] =
                py::make_tuple(entry.second.first, entry.second.second);
        }
        for (const auto &entry : nodes_) {
            result[py::int_(entry.first)] = py::make_tuple(entry.second.first, entry.second.second);
        }
        return result;
    }

private:
    std::unordered_map<int, Position> nodes_;
    std::unordered_map<int, Position> defaults_;
};

using ExecChildrenMap = std::unordered_map<int, std::vector<int>>;
using ExecDepthMap = std::unordered_map<int, int>;
using PinProviderList = std::vector<std::optional<int>>;
using NodeRelyMap = std::unordered_map<int, PinProviderList>;

double resolveStartAnchorY(int defaultParamCount, const GraphLayoutOptions &options) {
    if (defaultParamCount <= 0) {
        return 0.0;
    }
    return options.defaultParamStartY + (defaultParamCount - 1) * options.defaultParamYStep + options.startGap;
}

std::optional<int> resolveProviderIndex(const py::handle &leftRef) {
    if (py::isinstance<py::int_>(leftRef)) {
        return leftRef.cast<int>();
    }
    if (py::isinstance<py::tuple>(leftRef)) {
        py::tuple tupleRef = py::reinterpret_borrow<py::tuple>(leftRef);
        if (tupleRef.size() > 0 && py::isinstance<py::int_>(tupleRef[0])) {
            return tupleRef[0].cast<int>();
        }
    }
    return std::nullopt;
}

ExecChildrenMap buildExecChildrenMap(const py::list &links) {
    ExecChildrenMap children;
    std::unordered_map<int, std::unordered_set<int>> seen;
    for (py::handle item : links) {
        py::dict link = py::cast<py::dict>(item);
        if (!link.contains("linkType")) {
            continue;
        }
        py::object linkType = py::reinterpret_borrow<py::object>(link["linkType"]);
        if (!py::isinstance<py::str>(linkType) || linkType.cast<std::string>() != "Exec") {
            continue;
        }
        if (!link.contains("left") || !link.contains("right")) {
            continue;
        }
        py::object leftObj = py::reinterpret_borrow<py::object>(link["left"]);
        py::object rightObj = py::reinterpret_borrow<py::object>(link["right"]);
        if (!py::isinstance<py::int_>(leftObj) || !py::isinstance<py::int_>(rightObj)) {
            continue;
        }
        int left = leftObj.cast<int>();
        int right = rightObj.cast<int>();
        if (seen[left].find(right) != seen[left].end()) {
            continue;
        }
        seen[left].insert(right);
        children[left].push_back(right);
    }
    return children;
}

ExecDepthMap computeExecDepths(int startIdx, const ExecChildrenMap &execChildren) {
    ExecDepthMap depths;
    depths[startIdx] = 0;
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &entry : execChildren) {
            int left = entry.first;
            auto depthIt = depths.find(left);
            if (depthIt == depths.end()) {
                continue;
            }
            for (int right : entry.second) {
                int newDepth = depthIt->second + 1;
                auto rightIt = depths.find(right);
                if (rightIt == depths.end() || rightIt->second < newDepth) {
                    depths[right] = newDepth;
                    changed = true;
                }
            }
        }
    }
    return depths;
}

void layoutExecTree(int startIdx, double startY, int nodeCount, const ExecChildrenMap &execChildren,
                    const ExecDepthMap &execDepths, PositionStore &positions, const GraphLayoutOptions &options) {
    std::unordered_set<int> visited;

    const std::function<void(int, double)> assignY = [&](int nodeIdx, double y) {
        if (visited.find(nodeIdx) != visited.end() || nodeIdx < 0 || nodeIdx >= nodeCount) {
            return;
        }
        visited.insert(nodeIdx);
        auto depthIt = execDepths.find(nodeIdx);
        double x = (depthIt != execDepths.end() ? depthIt->second : 0) * options.xStep;
        positions.setNode(nodeIdx, x, y);

        auto childIt = execChildren.find(nodeIdx);
        if (childIt == execChildren.end()) {
            return;
        }
        const std::vector<int> &childList = childIt->second;
        if (childList.size() == 1) {
            assignY(childList[0], y);
        } else if (childList.size() > 1) {
            double branchY = y - (static_cast<int>(childList.size()) - 1) * options.yStep / 2.0;
            for (std::size_t index = 0; index < childList.size(); ++index) {
                assignY(childList[index], branchY + static_cast<double>(index) * options.yStep);
            }
        }
    };

    assignY(startIdx, startY);
}

NodeRelyMap buildNodeRelyMap(const py::dict &nodeRely) {
    NodeRelyMap result;
    for (auto item : nodeRely) {
        int consumerIdx = py::cast<int>(item.first);
        py::dict pins = py::cast<py::dict>(item.second);
        std::vector<std::pair<py::object, std::optional<int>>> entries;
        entries.reserve(static_cast<std::size_t>(pins.size()));
        for (auto pinItem : pins) {
            py::object pinKey = py::reinterpret_borrow<py::object>(pinItem.first);
            py::object leftRef = py::reinterpret_borrow<py::object>(pinItem.second);
            entries.emplace_back(pinKey, resolveProviderIndex(leftRef));
        }
        std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
            return left.first.attr("__lt__")(right.first).template cast<bool>();
        });
        PinProviderList providers;
        providers.reserve(entries.size());
        for (const auto &entry : entries) {
            providers.push_back(entry.second);
        }
        result[consumerIdx] = std::move(providers);
    }
    return result;
}

double resolveNodeHeight(int nodeIdx, int nodeCount, const std::vector<double> &nodeHeights,
                         const GraphLayoutOptions &options) {
    if (nodeIdx >= 0 && nodeIdx < nodeCount &&
        nodeIdx < static_cast<int>(nodeHeights.size()) &&
        nodeHeights[static_cast<std::size_t>(nodeIdx)] > 0.0) {
        return nodeHeights[static_cast<std::size_t>(nodeIdx)];
    }
    return options.yStep;
}

void ensureParamProvider(int providerIdx, int consumerIdx, double laneY, const NodeRelyMap &nodeRely,
                         PositionStore &positions, int &nextLane, double startY,
                         const GraphLayoutOptions &options) {
    if (!positions.hasNode(consumerIdx)) {
        return;
    }
    const Position &consumerPos = positions.node(consumerIdx);
    double targetX = consumerPos.first - options.xStep;
    if (positions.hasNode(providerIdx)) {
        Position &providerPos = positions.node(providerIdx);
        providerPos.first = std::min(providerPos.first, targetX);
    } else {
        positions.setNode(providerIdx, targetX, laneY);
    }

    auto relyIt = nodeRely.find(providerIdx);
    if (relyIt == nodeRely.end()) {
        return;
    }
    bool firstProvider = true;
    double childLaneY = laneY;
    for (const auto &leftIdx : relyIt->second) {
        if (leftIdx.has_value()) {
            if (!firstProvider) {
                ++nextLane;
                childLaneY = startY + nextLane * options.yStep;
            }
            ensureParamProvider(leftIdx.value(), providerIdx, childLaneY, nodeRely, positions, nextLane, startY,
                                options);
            firstProvider = false;
        }
    }
}

void layoutParamProviders(const NodeRelyMap &nodeRely, PositionStore &positions, double startY,
                          const GraphLayoutOptions &options) {
    std::vector<int> execNodes = positions.sortedNodeKeysByPos();
    std::unordered_map<int, double> laneYByConsumer;
    int nextLane = 0;
    for (int consumerIdx : execNodes) {
        auto relyIt = nodeRely.find(consumerIdx);
        if (relyIt == nodeRely.end() || relyIt->second.empty()) {
            continue;
        }
        ++nextLane;
        laneYByConsumer[consumerIdx] = startY + nextLane * options.yStep;
    }

    std::vector<int> laneConsumers;
    laneConsumers.reserve(laneYByConsumer.size());
    for (const auto &entry : laneYByConsumer) {
        laneConsumers.push_back(entry.first);
    }
    std::sort(laneConsumers.begin(), laneConsumers.end(), [&positions](int left, int right) {
        return positions.node(left).first < positions.node(right).first;
    });
    for (int consumerIdx : laneConsumers) {
        double laneY = laneYByConsumer.at(consumerIdx);
        bool firstProvider = true;
        for (const auto &leftIdx : nodeRely.at(consumerIdx)) {
            if (leftIdx.has_value()) {
                if (!firstProvider) {
                    ++nextLane;
                    laneY = startY + nextLane * options.yStep;
                }
                ensureParamProvider(leftIdx.value(), consumerIdx, laneY, nodeRely, positions, nextLane, startY,
                                    options);
                firstProvider = false;
            }
        }
    }

    std::vector<int> remainingNodes;
    for (int consumerIdx : execNodes) {
        if (laneYByConsumer.find(consumerIdx) == laneYByConsumer.end()) {
            remainingNodes.push_back(consumerIdx);
        }
    }
    std::sort(remainingNodes.begin(), remainingNodes.end(), [&positions](int left, int right) {
        return positions.node(left).first > positions.node(right).first;
    });
    for (int consumerIdx : remainingNodes) {
        auto relyIt = nodeRely.find(consumerIdx);
        if (relyIt == nodeRely.end() || relyIt->second.empty()) {
            continue;
        }
        double laneY = 0.0;
        for (const auto &leftIdx : relyIt->second) {
            if (leftIdx.has_value()) {
                ++nextLane;
                laneY = startY + nextLane * options.yStep;
                ensureParamProvider(leftIdx.value(), consumerIdx, laneY, nodeRely, positions, nextLane, startY,
                                    options);
            }
        }
    }
}

void layoutRemainingNodes(int nodeCount, PositionStore &positions, double startY, int paramLaneCount,
                          const GraphLayoutOptions &options) {
    double maxX = 0.0;
    for (int idx : positions.nodeKeys()) {
        maxX = std::max(maxX, positions.node(idx).first);
    }
    double orphanY = startY + (paramLaneCount + 1) * options.yStep;
    for (int idx = 0; idx < nodeCount; ++idx) {
        if (positions.hasNode(idx)) {
            continue;
        }
        positions.setNode(idx, maxX + options.xStep, orphanY);
        orphanY += options.yStep;
    }
}

int countExecNodeRely(const PositionStore &positions, const NodeRelyMap &nodeRely) {
    int count = 0;
    for (int key : positions.nodeKeys()) {
        auto relyIt = nodeRely.find(key);
        if (relyIt != nodeRely.end() && !relyIt->second.empty()) {
            ++count;
        }
    }
    return count;
}

int computeParamLaneCount(const PositionStore &positions, double startAnchorY, int execNodeCount,
                          const GraphLayoutOptions &options) {
    int paramLaneCount = execNodeCount;
    for (int key : positions.nodeKeys()) {
        double posY = positions.node(key).second;
        if (posY > startAnchorY + 1.0) {
            int laneIndex = static_cast<int>(std::lround((posY - startAnchorY) / options.yStep));
            paramLaneCount = std::max(paramLaneCount, laneIndex);
        }
    }
    return paramLaneCount;
}

void resolveColumnOverlaps(PositionStore &positions, int nodeCount, const std::vector<double> &nodeHeights,
                           const GraphLayoutOptions &options) {
    std::vector<int> keys = positions.sortedNodeKeysByPos();
    if (keys.size() <= 1) {
        return;
    }

    std::vector<int> column;
    double columnX = positions.node(keys.front()).first;
    const auto resolveColumn = [&]() {
        if (column.size() <= 1) {
            return;
        }
        std::sort(column.begin(), column.end(), [&positions](int left, int right) {
            return positions.node(left).second < positions.node(right).second;
        });
        for (std::size_t index = 1; index < column.size(); ++index) {
            int upperIdx = column[index - 1];
            int lowerIdx = column[index];
            double minLowerY = positions.node(upperIdx).second + resolveNodeHeight(upperIdx, nodeCount, nodeHeights, options) +
                               options.columnPadding;
            double &lowerY = positions.node(lowerIdx).second;
            if (lowerY < minLowerY) {
                lowerY = minLowerY;
            }
        }
    };

    for (int key : keys) {
        double x = positions.node(key).first;
        if (column.empty() || std::abs(x - columnX) < 1.0) {
            if (column.empty()) {
                columnX = x;
            }
            column.push_back(key);
            continue;
        }
        resolveColumn();
        column = {key};
        columnX = x;
    }
    resolveColumn();
}

} // namespace

py::dict C_ComputeGraphLayoutPositions(int nodeCount, py::list links, py::dict nodeRely, py::object startIdx,
                                       int defaultParamCount, const std::vector<double> &nodeHeights, double xStep,
                                       double yStep, double defaultParamYStep, double defaultParamStartY, double startGap,
                                       double columnPadding) {
    GraphLayoutOptions options{xStep, yStep, defaultParamYStep, defaultParamStartY, startGap, columnPadding};
    PositionStore positions;
    for (int idx = 0; idx < defaultParamCount; ++idx) {
        positions.setDefault(idx, 0.0, options.defaultParamStartY + idx * options.defaultParamYStep);
    }

    double startAnchorY = resolveStartAnchorY(defaultParamCount, options);
    ExecChildrenMap execChildren = buildExecChildrenMap(links);
    std::optional<int> resolvedStartIdx;
    if (!startIdx.is_none()) {
        resolvedStartIdx = startIdx.cast<int>();
    }

    if (resolvedStartIdx.has_value()) {
        ExecDepthMap execDepths = computeExecDepths(resolvedStartIdx.value(), execChildren);
        layoutExecTree(resolvedStartIdx.value(), startAnchorY, nodeCount, execChildren, execDepths, positions, options);
    } else if (nodeCount > 0) {
        double orphanY = startAnchorY;
        for (int idx = 0; idx < nodeCount; ++idx) {
            positions.setNode(idx, 0.0, orphanY);
            orphanY += options.yStep;
        }
    }

    NodeRelyMap parsedNodeRely = buildNodeRelyMap(nodeRely);
    int execNodeCount = countExecNodeRely(positions, parsedNodeRely);
    layoutParamProviders(parsedNodeRely, positions, startAnchorY, options);
    int paramLaneCount = computeParamLaneCount(positions, startAnchorY, execNodeCount, options);
    layoutRemainingNodes(nodeCount, positions, startAnchorY, paramLaneCount, options);
    resolveColumnOverlaps(positions, nodeCount, nodeHeights, options);
    return positions.toDict();
}
