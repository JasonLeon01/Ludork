#include "Particles/ParticleBase.hpp"

#include "Particles/ParticleSystem.hpp"

#include <utility>

ParticleBase::ParticleBase(
    std::shared_ptr<ParticleSystem> parent,
    std::function<void(float, float, ParticleBase*)> moveFunction,
    float countTime)
    : parent_(std::move(parent)),
      moveFunction_(std::move(moveFunction)),
      countTime_(countTime) {}

void ParticleBase::setParent(std::shared_ptr<ParticleSystem> parent) {
    parent_ = std::move(parent);
}

void ParticleBase::onTick(float deltaTime) {
    if (!moveFunction_) {
        return;
    }
    countTime_ += deltaTime;
    moveFunction_(deltaTime, countTime_, this);
}

float ParticleBase::getCountTime() const {
    return countTime_;
}

std::shared_ptr<ParticleSystem> ParticleBase::getParent() const {
    return parent_.lock();
}
