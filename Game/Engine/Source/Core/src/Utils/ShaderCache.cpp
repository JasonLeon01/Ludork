#include <Utils/Inner.hpp>
#include <Runtime/Graphics/ShaderCache.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>

#include <mutex>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace {

std::mutex requestMutex;
std::unordered_map<lua_State*, std::weak_ptr<ludork::engine::GraphicsCachePlan>>
    plans;
std::unordered_map<
    lua_State*,
    std::pair<std::vector<std::vector<std::string>>,
              std::weak_ptr<ludork::engine::shaders::ShaderCacheRequest>>>
    requests;

void shutdownShaderCache(lua_State* state) noexcept {
    std::shared_ptr<ludork::engine::GraphicsCachePlan> plan;
    std::shared_ptr<ludork::engine::shaders::ShaderCacheRequest> request;
    {
        std::lock_guard lock(requestMutex);
        if (const auto found = plans.find(state); found != plans.end()) {
            plan = found->second.lock();
            plans.erase(found);
        }
        const auto found = requests.find(state);
        if (found != requests.end()) {
            request = found->second.second.lock();
            requests.erase(found);
        }
    }
    if (plan) {
        plan->cancel();
    }
    if (request) {
        request->cancel();
    }
    ludork::runtime::graphics::ShaderCache::clear();
}

}  // namespace

std::shared_ptr<ludork::engine::shaders::ShaderCacheRequest> prepareShaderCache(
    const std::vector<std::vector<std::string>>& additionalPrograms) {
    ludork::runtime::RuntimeScope runtime;
    auto normalized = additionalPrograms;
    for (auto& program : normalized) {
        std::sort(program.begin(), program.end());
    }
    std::sort(normalized.begin(), normalized.end());
    normalized.erase(std::unique(normalized.begin(), normalized.end()),
                     normalized.end());
    const auto root = ludork::standard::pathFromUtf8(getUserDataPath());
    ludork::standard::registerRuntimeCleanup(runtime.state(),
                                             shutdownShaderCache);
    std::lock_guard lock(requestMutex);
    auto& active = requests[runtime.state()];
    if (auto existing = active.second.lock();
        existing && existing->getStatus() == "pending") {
        if (active.first != normalized) {
            throw std::logic_error(
                "Shader preparation is already pending with a different "
                "program list");
        }
        return existing;
    }
    auto request = ludork::engine::shaders::ShaderCacheRequest::create(
        root, additionalPrograms);
    active = {std::move(normalized), request};
    return request;
}

std::shared_ptr<ludork::engine::GraphicsCachePlan> planGraphicsCaches() {
    ludork::runtime::RuntimeScope runtime;
    const auto root = ludork::standard::pathFromUtf8(getUserDataPath());
    ludork::standard::registerRuntimeCleanup(runtime.state(),
                                             shutdownShaderCache);
    std::lock_guard lock(requestMutex);
    auto& active = plans[runtime.state()];
    if (auto existing = active.lock()) {
        const auto status = existing->getStatus();
        if (status != "ready" && status != "failed" && status != "cancelled") {
            return existing;
        }
    }
    auto plan = std::make_shared<ludork::engine::GraphicsCachePlan>(root);
    active = plan;
    return plan;
}
