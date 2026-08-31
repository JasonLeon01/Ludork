#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ludork::global::focus_manager_impl {

using ElementId = std::uintptr_t;
using GroupId = std::uintptr_t;

struct NeighborState {
    GroupId target = 0;
    std::string transition;
};

struct GroupState {
    std::vector<ElementId> elements;
    ElementId activeOwner = 0;
    ElementId remembered = 0;
    std::unordered_map<std::string, NeighborState> neighbors;
};

class FocusRuntime {
public:
    void addGroup(GroupId id, GroupState group);
    std::vector<GroupId> directionalPath(GroupId start,
                                         const std::string& direction) const;

private:
    std::unordered_map<GroupId, GroupState> groups_;
};

}  // namespace ludork::global::focus_manager_impl
