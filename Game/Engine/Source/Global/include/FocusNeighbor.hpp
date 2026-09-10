#pragma once

#include <CoreMinimal.hpp>

class FocusGroup;

BIND_CLASS()
class FocusNeighbor {
public:
    BIND_INIT(defaults = {directional})
    FocusNeighbor(std::shared_ptr<FocusGroup> group,
                  std::string transition = "directional");

    BIND_METHOD(property = "group", setter = "setGroup")
    std::shared_ptr<FocusGroup> getGroup() const;

    void setGroup(const std::shared_ptr<FocusGroup>& group);

    BIND_PROPERTY()
    std::string transition;

private:
    std::weak_ptr<FocusGroup> group_;
};
