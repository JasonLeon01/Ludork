#pragma once

#include <CoreMinimal.hpp>

#include <RuntimeApi.hpp>
#include <Runtime/NodeGraph/Types.hpp>
#include <Runtime/Async/AsyncOperation.hpp>

class Graph;

BIND_CLASS(metadata = false, bind_bases = false)
class LUDORK_RUNTIME_API LatentManager
    : public std::enable_shared_from_this<LatentManager> {
public:
    BIND_INIT()
    LatentManager() = default;

    BIND_METHOD(metadata = false)
    void add(const std::shared_ptr<Graph>& graph, const std::string& key,
             std::shared_ptr<AsyncOperation> operation,
             RuntimeIdentityPtr localRef, int index, NodeCache cache);

    BIND_METHOD(metadata = false)
    void update();

    BIND_METHOD(metadata = false)
    void cancel(const std::shared_ptr<AsyncOperation>& operation);

    bool isInitialised() const noexcept;
    void setInitialised(bool value) noexcept;
    void clear() noexcept;

private:
    struct Entry {
        std::weak_ptr<Graph> graph;
        std::string key;
        std::shared_ptr<AsyncOperation> operation;
        std::size_t nextEvent = 0;
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

LUDORK_RUNTIME_API LatentManager& latentManager();

BIND_MODULE_PROPERTY(name = "latentManager", readonly = true, cache = true,
                     metadata = false)
extern LUDORK_RUNTIME_API const std::shared_ptr<LatentManager>
    latentManagerInstance;
