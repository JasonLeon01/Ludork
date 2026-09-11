#pragma once

#include <Runtime/WebViewHost.hpp>

#include <atomic>
#include <mutex>

namespace ludork::runtime::webview {

struct WebViewServiceImpl {
    std::recursive_mutex operations;
    std::mutex mutex;
    HostFactory factory;
    std::shared_ptr<Host> host;
    std::uint64_t nextId = 0;
    std::uint64_t activeId = 0;
    std::uint64_t pendingPreviousId = 0;
    bool pendingPreviousClosed = false;
    std::atomic<bool> blocked = false;
    std::atomic<std::uint64_t> revision = 0;

    void clearSession();
    void finishRequest(std::uint64_t id, bool accepted);
};

}  // namespace ludork::runtime::webview
