#include "Particles/Particle.hpp"

#include "Particles/ParticleSystem.hpp"

#include <utility>

Particle::Particle(
    std::shared_ptr<ParticleSystem> parent,
    std::function<void(float, float, ParticleBase*)> moveFunction,
    float countTime, const std::string& inResourcePath, ParticleInfo inInfo)
    : ParticleBase(std::move(parent), std::move(moveFunction), countTime),
      info(inInfo),
      resourcePath(inResourcePath) {
    lastPosition_ = info.position;
    lastRotation_ = info.rotation;
    lastScale_ = info.scale;
    lastColor_ = info.color;
}

void Particle::onTick(float deltaTime) {
    const std::shared_ptr<ParticleSystem> parent = parent_.lock();
    if (parent == nullptr) {
        return;
    }
    ParticleBase::onTick(deltaTime);
    checkUpdate();
}

void Particle::onLateTick(float deltaTime) {
    const std::shared_ptr<ParticleSystem> parent = parent_.lock();
    if (parent == nullptr) {
        return;
    }
    ParticleBase::onLateTick(deltaTime);
    checkUpdate();
}

void Particle::onFixedTick(float fixedDelta) {
    const std::shared_ptr<ParticleSystem> parent = parent_.lock();
    if (parent == nullptr) {
        return;
    }
    ParticleBase::onFixedTick(fixedDelta);
    checkUpdate();
}

void Particle::destroy() {
    const std::shared_ptr<ParticleSystem> parent = parent_.lock();
    if (parent != nullptr) {
        parent->removeParticle(this);
    }
}

void Particle::checkUpdate() {
    bool updateFlag = false;
    if (lastPosition_ != info.position) {
        lastPosition_ = info.position;
        updateFlag = true;
    }
    if (lastRotation_ != info.rotation) {
        lastRotation_ = info.rotation;
        updateFlag = true;
    }
    if (lastScale_ != info.scale) {
        lastScale_ = info.scale;
        updateFlag = true;
    }
    if (lastColor_ != info.color) {
        lastColor_ = info.color;
        updateFlag = true;
    }
    if (updateFlag) {
        const std::shared_ptr<ParticleSystem> parent = parent_.lock();
        if (parent != nullptr) {
            parent->addUpdateFlag(this);
        }
    }
}
