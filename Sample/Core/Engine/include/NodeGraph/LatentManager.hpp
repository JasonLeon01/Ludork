#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <EngineRuntimeApi.hpp>
#include <NodeGraph/Types.hpp>
#include <Runtime/RuntimeValue.hpp>

#include <memory>
#include <string>
#include <vector>

class Graph;

BIND_CLASS(metadata = false)
class LUDORK_ENGINE_API LatentManager {
public:
    BIND_INIT()
    LatentManager() = default;

    BIND_METHOD(metadata = false)
    void add(const std::shared_ptr<Graph>& graph, const std::string& key,
             RuntimeIdentityPtr condition, RuntimeIdentityPtr localRef,
             int index, NodeCache cache);

    BIND_METHOD(metadata = false)
    void update();

    bool isInitialised() const noexcept;
    void setInitialised(bool value) noexcept;
    void clear() noexcept;

private:
    struct Entry {
        std::weak_ptr<Graph> graph;
        std::string key;
        RuntimeIdentityPtr condition;
        RuntimeIdentityPtr localRef;
        int index = 0;
        NodeCache cache;
    };

    void removeLatentsForNode(const std::shared_ptr<Graph>& graph,
                              const std::string& key, int index);

    std::vector<std::shared_ptr<Entry>> entries_;
    bool initialised_ = false;
    bool updating_ = false;
};

LUDORK_ENGINE_API LatentManager& latentManager();

LUDORK_ENGINE_API void initializeLatent();

LUDORK_ENGINE_API void shutdownLatent() noexcept;

BIND_MODULE_PROPERTY(name = "latentManager", readonly = true, cache = true,
                     metadata = false)
extern LUDORK_ENGINE_API const std::shared_ptr<LatentManager>
    latentManagerInstance;
