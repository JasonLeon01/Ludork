#include "LoopRuntime.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ludork::engine::graph_detail {

std::optional<int> namedExecPinIndex(const NodeMemberMetadata& metadata,
                                     const std::string& pinName) {
    for (std::size_t index = 0; index < metadata.execSplits.size(); ++index) {
        if (metadata.execSplits[index].name == pinName) {
            return static_cast<int>(index);
        }
    }
    return std::nullopt;
}

NodeResult emptyLoopResult(const NodeMemberMetadata& metadata) {
    if (metadata.loopNode == "ForEach") {
        return NodeResult{{RuntimeValue(), RuntimeValue(std::int64_t{-1})}, 2};
    }
    return NodeResult{{RuntimeValue(std::int64_t{0})}, 1};
}

std::vector<NodeResult> loopResults(const NodeMemberMetadata& metadata,
                                    const NodeResult& controlResult) {
    std::vector<NodeResult> result;
    if (metadata.loopNode == "ForEach") {
        if (controlResult.count == 0 || controlResult.values.empty()) {
            return result;
        }
        if (const RuntimeValue::Array* items =
                controlResult.values.front().getIf<RuntimeValue::Array>()) {
            result.reserve(items->size());
            for (std::size_t index = 0; index < items->size(); ++index) {
                result.push_back(
                    NodeResult{{(*items)[index],
                                RuntimeValue(static_cast<std::int64_t>(index))},
                               2});
            }
        } else if (const RuntimeValue::Map* items =
                       controlResult.values.front()
                           .getIf<RuntimeValue::Map>()) {
            result.reserve(items->size());
            std::size_t index = 0;
            for (const auto& [_, value] : *items) {
                result.push_back(NodeResult{
                    {value, RuntimeValue(static_cast<std::int64_t>(index))},
                    2});
                ++index;
            }
        }
        return result;
    }
    if (metadata.loopNode != "ForLoop") {
        return result;
    }

    const auto integerAt = [&controlResult](std::size_t index,
                                            std::int64_t fallback) {
        if (index >= controlResult.count ||
            index >= controlResult.values.size()) {
            return fallback;
        }
        const RuntimeValue& value = controlResult.values[index];
        if (const std::int64_t* integer = value.getIf<std::int64_t>()) {
            return *integer;
        }
        if (const double* number = value.getIf<double>()) {
            return static_cast<std::int64_t>(std::floor(*number));
        }
        return fallback;
    };
    const std::int64_t first = integerAt(0, 0);
    const std::int64_t last = integerAt(1, 0);
    const std::int64_t step = integerAt(2, 1);
    if (step == 0) {
        throw std::runtime_error("ForLoop step cannot be 0");
    }
    for (std::int64_t current = first;
         step > 0 ? current <= last : current >= last; current += step) {
        result.push_back(NodeResult{{RuntimeValue(current)}, 1});
        if ((step > 0 &&
             current > std::numeric_limits<std::int64_t>::max() - step) ||
            (step < 0 &&
             current < std::numeric_limits<std::int64_t>::min() - step)) {
            break;
        }
    }
    return result;
}

}  // namespace ludork::engine::graph_detail
