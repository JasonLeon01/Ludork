#include <LudorkGenerated/ServerConfig.hpp>

#if defined(LUDORK_SERVER_AVAILABLE) && defined(__EMSCRIPTEN__)
#include "ServerWebRequestImpl.hpp"
#include "ServerHttpTransportImpl.hpp"

#include <emscripten.h>

#include <algorithm>
#include <exception>
#include <utility>

namespace ludork::global::server_impl {
namespace {

// clang-format off
EM_JS(int, startBrowserRequest, (const char* url, int timeout, int maximumBytes), {
    var requests = Module['ludorkServerRequests'];
    if (!requests) {
        requests = { next: 1, pending: new Map() };
        Module['ludorkServerRequests'] = requests;
    }
    var id = requests.next++;
    var request = { controller: new AbortController(), state: 0, status: 0, bytes: null, timer: null };
    requests.pending.set(id, request);
    if (globalThis.location?.protocol === 'https:' && !UTF8ToString(url).startsWith('https://')) {
        request.state = 5;
        return id;
    }
    request.timer = setTimeout(() => {
        if (requests.pending.get(id) === request && request.state === 0) {
            request.state = 2;
            request.controller.abort();
        }
    }, timeout);
    fetch(UTF8ToString(url), { method: 'GET', credentials: 'omit', mode: 'cors', redirect: 'error', cache: 'no-store', signal: request.controller.signal })
    .then(async response => {
        if (Number(response.headers.get('Content-Length')) > maximumBytes) {
            request.state = 4;
            request.controller.abort();
            return;
        }
        var chunks = [];
        var size = 0;
        if (response.body) {
            var reader = response.body.getReader();
            for (;;) {
                var chunk = await reader.read();
                if (requests.pending.get(id) !== request || request.state !== 0) {
                    await reader.cancel();
                    return;
                }
                if (chunk.done) break;
                size += chunk.value.length;
                if (size > maximumBytes) {
                    request.state = 4;
                    await reader.cancel();
                    return;
                }
                chunks.push(chunk.value);
            }
        }
        if (requests.pending.get(id) !== request || request.state !== 0) return;
        request.bytes = new Uint8Array(size);
        var offset = 0;
        for (var chunk of chunks) {
            request.bytes.set(chunk, offset);
            offset += chunk.length;
        }
        request.status = response.status;
        request.state = 1;
    }).catch(() => {
        if (requests.pending.get(id) === request && request.state === 0) request.state = 3;
    }).finally(() => clearTimeout(request.timer));
    return id;
});

EM_JS(int, browserRequestState, (int id), {
    return Module['ludorkServerRequests'].pending.get(id)?.state ?? 3;
});

EM_JS(int, browserRequestStatus, (int id), {
    return Module['ludorkServerRequests'].pending.get(id)?.status ?? 0;
});

EM_JS(int, browserRequestLength, (int id), {
    return Module['ludorkServerRequests'].pending.get(id)?.bytes?.length ?? 0;
});

EM_JS(void, copyBrowserResponse, (int id, char* destination), {
    var bytes = Module['ludorkServerRequests'].pending.get(id)?.bytes;
    if (bytes) HEAPU8.set(bytes, destination);
});

EM_JS(void, releaseBrowserRequest, (int id), {
    var requests = Module['ludorkServerRequests'];
    var request = requests?.pending.get(id);
    if (request) {
        requests.pending.delete(id);
        clearTimeout(request.timer);
        request.controller.abort();
    }
});
// clang-format on

}  // namespace

ServerWebRequestImpl::ServerWebRequestImpl(
    std::shared_ptr<PendingRequest> pending)
    : pending_(std::move(pending)) {
    project_ = ServerHttpTransportImpl::parseEndpoint().project;
    std::string url = ludork::generated::server::Url;
    while (url.ends_with('/')) {
        url.pop_back();
    }
    url += "/api/v1" + pending_->request.path;
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            pending_->deadline - PendingRequest::Clock::now())
            .count();
    handle_ = startBrowserRequest(
        url.c_str(),
        static_cast<int>(std::clamp<std::int64_t>(remaining, 1, 3000)),
        2 * 1024 * 1024);
}

ServerWebRequestImpl::~ServerWebRequestImpl() {
    if (handle_ != 0) {
        releaseBrowserRequest(handle_);
    }
}

bool ServerWebRequestImpl::poll() {
    if (pending_->cancelled.load()) {
        return true;
    }
    int state = browserRequestState(handle_);
    if (PendingRequest::Clock::now() >= pending_->deadline) {
        state = 2;
    }
    if (state == 0) {
        return false;
    }
    Response response;
    if (state == 1) {
        try {
            std::string bytes(
                static_cast<std::size_t>(browserRequestLength(handle_)), '\0');
            copyBrowserResponse(handle_, bytes.data());
            response = ServerHttpTransportImpl::decodeResponse(
                *pending_, project_, browserRequestStatus(handle_), bytes);
        } catch (const std::exception&) {
            response = {ServerErrorCode::InvalidResponse,
                        "The server returned an invalid response."};
        }
    } else if (state == 2) {
        response = {ServerErrorCode::Timeout, "The server request timed out."};
    } else if (state == 5) {
        response = {ServerErrorCode::ConnectionFailed,
                    "HTTPS pages require an HTTPS server URL."};
    } else if (state == 4) {
        response = {ServerErrorCode::InvalidResponse,
                    "The server response exceeds its limit."};
    } else {
        response = {ServerErrorCode::ConnectionFailed,
                    "The server request could not be completed."};
    }
    std::lock_guard lock(pending_->mutex);
    if (!pending_->cancelled.load()) {
        pending_->response = std::move(response);
    }
    return true;
}

}  // namespace ludork::global::server_impl
#endif
