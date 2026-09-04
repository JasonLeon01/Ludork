#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <EngineRuntimeApi.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

class Actor;

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API ActorUpdateBatch {
public:
    BIND_INIT()
    ActorUpdateBatch();

    BIND_METHOD(metadata = false)
    void syncActors(const std::vector<std::shared_ptr<Actor>>& actors);

    BIND_METHOD(metadata = false)
    void update(float deltaTime);

    BIND_METHOD(metadata = false)
    void lateUpdate(float deltaTime);

    BIND_METHOD(metadata = false)
    void fixedUpdate(float fixedDelta);

private:
    std::vector<std::shared_ptr<Actor>> actors_;
    std::unordered_map<Actor*, unsigned int> tickEvents_;
};
