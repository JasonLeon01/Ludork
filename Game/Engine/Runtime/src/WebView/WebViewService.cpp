#include <Runtime/WebView.hpp>
#include <Runtime/WebViewService.hpp>

#include "Platform/WebViewPlatform.hpp"
#include "WebViewServiceImpl.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace {

ludork::runtime::webview::WebViewServiceImpl& state() {
    static ludork::runtime::webview::WebViewServiceImpl instance;
    return instance;
}

bool validUrl(const std::string& url) {
    const std::size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) {
        return false;
    }
    std::string scheme = url.substr(0, schemeEnd);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), [](char ch) {
        return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + 'a' - 'A') : ch;
    });
    if (scheme != "http" && scheme != "https") {
        return false;
    }
    const std::string_view authority(url.data() + schemeEnd + 3,
                                     url.size() - schemeEnd - 3);
    return !authority.empty() && authority.front() != '/' &&
           authority.front() != '?' && authority.front() != '#' &&
           std::none_of(url.begin(), url.end(), [](unsigned char ch) {
               return ch <= 0x20 || ch == 0x7f || ch == '\\';
           });
}

}  // namespace

namespace ludork::runtime::webview {

void WebViewServiceImpl::clearSession() {
    activeId = 0;
    if (blocked.exchange(false)) {
        ++revision;
    }
}

void WebViewServiceImpl::finishRequest(std::uint64_t id, bool accepted) {
    if (!accepted && activeId == id) {
        activeId = pendingPreviousClosed ? 0 : pendingPreviousId;
        blocked = activeId != 0;
        ++revision;
    }
    pendingPreviousId = 0;
    pendingPreviousClosed = false;
}

void detachWindow() {
    const std::lock_guard operation(state().operations);
    std::shared_ptr<Host> previous;
    std::uint64_t id = 0;
    {
        const std::lock_guard lock(state().mutex);
        previous = std::move(state().host);
        id = state().activeId;
        state().clearSession();
    }
    if (previous != nullptr && id != 0) {
        previous->close(id);
    }
}

void setHostFactory(HostFactory factory) {
    const std::lock_guard operation(state().operations);
    detachWindow();
    const std::lock_guard lock(state().mutex);
    state().factory = std::move(factory);
}

void attachWindow(sf::WindowHandle window) {
    const std::lock_guard operation(state().operations);
    detachWindow();
    if (window == sf::WindowHandle{}) {
        return;
    }
    HostFactory factory;
    {
        const std::lock_guard lock(state().mutex);
        factory = state().factory;
    }
    std::shared_ptr<Host> host =
        factory ? factory(window) : createPlatformHost(window);
    const std::lock_guard lock(state().mutex);
    state().host = std::move(host);
}

void notifyClosed(std::uint64_t id) {
    const std::lock_guard lock(state().mutex);
    if (id != 0 && state().pendingPreviousId == id) {
        state().pendingPreviousClosed = true;
    }
    if (id != 0 && state().activeId == id) {
        state().clearSession();
    }
}

bool isInputBlocked() {
    return state().blocked.load();
}

std::uint64_t inputRevision() {
    return state().revision.load();
}

}  // namespace ludork::runtime::webview

namespace ludork::runtime {

bool OpenWebView(const std::string& url) {
    if (!validUrl(url)) {
        return false;
    }
    const std::lock_guard operation(state().operations);
    std::shared_ptr<webview::Host> host;
    std::uint64_t id = 0;
    {
        const std::lock_guard lock(state().mutex);
        host = state().host;
        if (host == nullptr) {
            return false;
        }
        state().pendingPreviousId = state().activeId;
        state().pendingPreviousClosed = false;
        state().activeId = ++state().nextId;
        id = state().activeId;
        state().blocked = true;
        ++state().revision;
    }
    bool accepted = false;
    try {
        accepted = host->open(id, url);
    } catch (...) {
        const std::lock_guard lock(state().mutex);
        state().finishRequest(id, false);
        throw;
    }
    {
        const std::lock_guard lock(state().mutex);
        state().finishRequest(id, accepted);
    }
    return accepted;
}

}  // namespace ludork::runtime
