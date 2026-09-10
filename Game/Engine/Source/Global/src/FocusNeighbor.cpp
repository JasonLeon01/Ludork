#include <FocusNeighbor.hpp>
#include <FocusGroup.hpp>

#include <utility>

FocusNeighbor::FocusNeighbor(std::shared_ptr<FocusGroup> groupValue,
                             std::string transitionValue)
    : group_(std::move(groupValue)), transition(std::move(transitionValue)) {}

std::shared_ptr<FocusGroup> FocusNeighbor::getGroup() const {
    return group_.lock();
}

void FocusNeighbor::setGroup(const std::shared_ptr<FocusGroup>& group) {
    group_ = group;
}
