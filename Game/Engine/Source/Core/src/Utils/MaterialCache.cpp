#include <Utils/Inner.hpp>
#include <Runtime/RuntimeSession.hpp>
#include <RuntimeSession.hpp>
#include <Utf8Path.hpp>

#include <mutex>
#include <unordered_map>

namespace {

std::mutex requestMutex;
std::unordered_map<
    lua_State*, std::weak_ptr<ludork::engine::materials::MaterialCacheRequest>>
    requests;

void shutdownMaterialCache(lua_State* state) noexcept {
    std::shared_ptr<ludork::engine::materials::MaterialCacheRequest> request;
    {
        std::lock_guard lock(requestMutex);
        const auto found = requests.find(state);
        if (found != requests.end()) {
            request = found->second.lock();
            requests.erase(found);
        }
    }
    if (request) {
        request->cancel();
    }
}

}  // namespace

std::shared_ptr<ludork::engine::materials::MaterialCacheRequest>
prepareMaterialCache() {
    ludork::runtime::RuntimeScope runtime;
    const auto root = ludork::standard::pathFromUtf8(getUserDataPath());
    ludork::standard::registerRuntimeCleanup(runtime.state(),
                                             shutdownMaterialCache);
    std::lock_guard lock(requestMutex);
    auto& active = requests[runtime.state()];
    if (auto existing = active.lock();
        existing && existing->getStatus() == "pending") {
        return existing;
    }
    auto request =
        ludork::engine::materials::MaterialCacheRequest::create(root);
    active = request;
    return request;
}
