#include "Particles/ParticleBase.hpp"

#include "Particles/ParticleSystem.hpp"

ParticleBase::ParticleBase(ParticleSystem* parent, std::function<void(float, float, ParticleBase*)> moveFunction,
                           float countTime) {
    parent_ = parent;
    moveFunction_ = moveFunction;
    countTime_ = countTime;
}

void ParticleBase::setParent(ParticleSystem* parent) { parent_ = parent; }

void ParticleBase::onTick(float deltaTime) {
    if (!moveFunction_) {
        return;
    }
    countTime_ += deltaTime;
    moveFunction_(deltaTime, countTime_, this);
}

float ParticleBase::getCountTime() const { return countTime_; }

ParticleSystem* ParticleBase::getParent() const { return parent_; }
