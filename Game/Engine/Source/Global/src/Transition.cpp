#include <Transition.hpp>
#include "System/FramePipelineImpl.hpp"
#include <utility>

std::shared_ptr<AsyncOperation> Transition::setTransition(
    const std::shared_ptr<sf::Texture>& transitionResource,
    float transitionTime) {
    return ludork::global::system_impl::framePipelineImpl()
        .transition()
        .setTransition(transitionResource, transitionTime);
}

std::shared_ptr<AsyncOperation> Transition::freezeTransitionBackground() {
    return ludork::global::system_impl::framePipelineImpl()
        .transition()
        .freezeTransitionBackground();
}

bool Transition::isTransitionBackgroundFrozen() {
    return ludork::global::system_impl::framePipelineImpl()
        .transition()
        .isTransitionBackgroundFrozen();
}

bool Transition::isTransitionBackgroundFreezePending() {
    return ludork::global::system_impl::framePipelineImpl()
        .transition()
        .isTransitionBackgroundFreezePending();
}

void Transition::cancelTransitionBackgroundFreeze() {
    ludork::global::system_impl::framePipelineImpl()
        .transition()
        .cancelTransitionBackgroundFreeze();
}

std::shared_ptr<AsyncOperation> Transition::requestTransition(
    std::optional<std::string> transitionName, float transitionTime) {
    return ludork::global::system_impl::framePipelineImpl()
        .transition()
        .requestTransition(std::move(transitionName), transitionTime);
}

void Transition::cancelPendingTransition() {
    ludork::global::system_impl::framePipelineImpl()
        .transition()
        .cancelPendingTransition();
}

bool Transition::isTransitionPending() {
    return ludork::global::system_impl::framePipelineImpl()
        .transition()
        .isTransitionPending();
}

bool Transition::isInTransition() {
    return ludork::global::system_impl::framePipelineImpl()
        .transition()
        .isInTransition();
}

void Transition::applyPendingTransition() {
    ludork::global::system_impl::framePipelineImpl()
        .transition()
        .applyPendingTransition();
}
