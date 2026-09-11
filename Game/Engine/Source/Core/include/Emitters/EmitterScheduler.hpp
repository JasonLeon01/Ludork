#pragma once

#include <Emitters/Emitter.hpp>
#include <EngineRuntimeApi.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class LUDORK_ENGINE_API EmitterScheduler
    : public std::enable_shared_from_this<EmitterScheduler> {
public:
    void beginFrame();
    void registerEmitter(const std::shared_ptr<Emitter>& emitter,
                         const sf::Transform& hostTransform);
    void advance(float deltaTime);
    void shutdown() noexcept;

private:
    std::unordered_map<Emitter*, std::weak_ptr<Emitter>> known_;
    std::unordered_set<Emitter*> registered_;
    std::unordered_set<Emitter*> previous_;
    std::vector<std::weak_ptr<Emitter>> active_;
};
