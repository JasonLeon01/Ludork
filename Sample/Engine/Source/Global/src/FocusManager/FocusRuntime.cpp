#include "FocusRuntime.hpp"

#include <unordered_set>

namespace ludork::global::focus_manager_impl {

void FocusRuntime::addGroup(GroupId id, GroupState group) {
    groups_.insert_or_assign(id, std::move(group));
}

std::vector<GroupId> FocusRuntime::directionalPath(
    GroupId start, const std::string& direction) const {
    std::vector<GroupId> result;
    std::unordered_set<GroupId> visited{start};
    GroupId current = start;
    while (current != 0) {
        const auto group = groups_.find(current);
        if (group == groups_.end()) {
            break;
        }
        const auto neighbor = group->second.neighbors.find(direction);
        if (neighbor == group->second.neighbors.end() ||
            neighbor->second.transition != "directional" ||
            neighbor->second.target == 0 ||
            !visited.insert(neighbor->second.target).second) {
            break;
        }
        current = neighbor->second.target;
        result.push_back(current);
    }
    return result;
}

}  // namespace ludork::global::focus_manager_impl
