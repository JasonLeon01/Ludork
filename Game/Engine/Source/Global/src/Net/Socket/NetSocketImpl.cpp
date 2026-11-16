#include "NetSocketImpl.hpp"

#if defined(LUDORK_SERVER_AVAILABLE)
#include <limits>
#include <stdexcept>
#include <utility>

namespace ludork::global::net_impl {

#if defined(__EMSCRIPTEN__)
namespace {
EM_JS_DEPS(browserSocketDependencies, "emscripten_websocket_new");
// clang-format off
EM_JS(bool, allowsBrowserSocket, (const char* url), {
    return globalThis.location?.protocol !== 'https:' || UTF8ToString(url).startsWith('wss://');
});

EM_JS(int, openBrowserSocket, (const EmscriptenWebSocketCreateAttributes* attributes), {
    try { return _emscripten_websocket_new(attributes); }
    catch { return 0; }
});
// clang-format on
}  // namespace
#endif

NetSocketImpl::NetSocketImpl(std::string url, std::uint64_t generation,
                             std::size_t maximumMessageBytes,
                             std::function<void(Notification)> callback)
    : url_(std::move(url)),
      generation_(generation),
      maximumMessageBytes_(maximumMessageBytes),
      callback_(std::move(callback)) {
#if !defined(__EMSCRIPTEN__)
    socket_ = std::make_unique<ix::WebSocket>();
    socket_->setUrl(url_);
    socket_->disablePerMessageDeflate();
    socket_->disableAutomaticReconnection();
    socket_->setHandshakeTimeout(10);
    socket_->setPingInterval(15);
    socket_->setOnMessageCallback(
        [this](const ix::WebSocketMessagePtr& message) {
            Notification notification;
            notification.generation = generation_;
            if (message->type == ix::WebSocketMessageType::Open) {
                notification.kind = Notification::Kind::Open;
            } else if (message->type == ix::WebSocketMessageType::Message) {
                notification.kind = message->str.size() > maximumMessageBytes_
                                        ? Notification::Kind::Overflow
                                        : Notification::Kind::Message;
                notification.binary = message->binary;
                if (notification.kind != Notification::Kind::Overflow) {
                    notification.bytes = message->str;
                }
            } else if (message->type == ix::WebSocketMessageType::Close) {
                notification.kind = Notification::Kind::Close;
                notification.status = message->closeInfo.code;
            } else if (message->type == ix::WebSocketMessageType::Error) {
                notification.kind = Notification::Kind::Error;
                notification.status = message->errorInfo.http_status;
            } else {
                return;
            }
            callback_(std::move(notification));
        });
#endif
}

NetSocketImpl::~NetSocketImpl() {
    stop();
}

void NetSocketImpl::start() {
#if defined(__EMSCRIPTEN__)
    if (!allowsBrowserSocket(url_.c_str())) {
        callback_({Notification::Kind::Error, generation_,
                   "HTTPS pages require a WSS server connection."});
        return;
    }
    EmscriptenWebSocketCreateAttributes attributes{};
    attributes.url = url_.c_str();
    attributes.createOnMainThread = true;
    socket_ = openBrowserSocket(&attributes);
    if (socket_ <= 0) {
        socket_ = 0;
        callback_({Notification::Kind::Error, generation_});
        return;
    }
    emscripten_websocket_set_onopen_callback(socket_, this, onOpen);
    emscripten_websocket_set_onmessage_callback(socket_, this, onMessage);
    emscripten_websocket_set_onclose_callback(socket_, this, onClose);
    emscripten_websocket_set_onerror_callback(socket_, this, onError);
#else
    socket_->start();
#endif
}

void NetSocketImpl::stop() {
#if defined(__EMSCRIPTEN__)
    if (socket_ > 0) {
        emscripten_websocket_set_onopen_callback(socket_, nullptr, nullptr);
        emscripten_websocket_set_onmessage_callback(socket_, nullptr, nullptr);
        emscripten_websocket_set_onclose_callback(socket_, nullptr, nullptr);
        emscripten_websocket_set_onerror_callback(socket_, nullptr, nullptr);
        emscripten_websocket_close(socket_, 1000, "");
        emscripten_websocket_delete(socket_);
        socket_ = 0;
    }
#else
    socket_->stop();
#endif
}

NetSocketImpl::State NetSocketImpl::getReadyState() const {
#if defined(__EMSCRIPTEN__)
    unsigned short state = 3;
    if (socket_ > 0) {
        emscripten_websocket_get_ready_state(socket_, &state);
    }
    return static_cast<State>(state);
#else
    switch (socket_->getReadyState()) {
        case ix::ReadyState::Connecting:
            return State::Connecting;
        case ix::ReadyState::Open:
            return State::Open;
        case ix::ReadyState::Closing:
            return State::Closing;
        case ix::ReadyState::Closed:
            return State::Closed;
    }
    return State::Closed;
#endif
}

std::size_t NetSocketImpl::bufferedAmount() const {
#if defined(__EMSCRIPTEN__)
    std::size_t amount = std::numeric_limits<std::size_t>::max();
    if (socket_ > 0) {
        emscripten_websocket_get_buffered_amount(socket_, &amount);
    }
    return amount;
#else
    return socket_->bufferedAmount();
#endif
}

bool NetSocketImpl::sendText(const std::string& text) {
#if defined(__EMSCRIPTEN__)
    return socket_ > 0 &&
           emscripten_websocket_send_utf8_text(socket_, text.c_str()) ==
               EMSCRIPTEN_RESULT_SUCCESS;
#else
    return socket_->sendText(text).success;
#endif
}

void NetSocketImpl::close(unsigned short code, const char* reason) {
#if defined(__EMSCRIPTEN__)
    if (socket_ > 0) {
        const unsigned short browserCode =
            code == 1000 || code >= 3000 ? code : 4000 + code % 1000;
        emscripten_websocket_close(socket_, browserCode, reason);
    }
#else
    socket_->close(code, reason);
#endif
}

#if defined(__EMSCRIPTEN__)
bool NetSocketImpl::onOpen(int, const EmscriptenWebSocketOpenEvent*,
                           void* data) {
    auto& self = *static_cast<NetSocketImpl*>(data);
    self.callback_({Notification::Kind::Open, self.generation_});
    return true;
}

bool NetSocketImpl::onMessage(int, const EmscriptenWebSocketMessageEvent* event,
                              void* data) {
    auto& self = *static_cast<NetSocketImpl*>(data);
    Notification notification;
    notification.generation = self.generation_;
    notification.binary = !event->isText;
    const std::size_t count = event->isText && event->numBytes > 0
                                  ? event->numBytes - 1
                                  : event->numBytes;
    if (count > self.maximumMessageBytes_) {
        notification.kind = Notification::Kind::Overflow;
    } else {
        notification.bytes.assign(reinterpret_cast<const char*>(event->data),
                                  count);
    }
    self.callback_(std::move(notification));
    return true;
}

bool NetSocketImpl::onClose(int, const EmscriptenWebSocketCloseEvent* event,
                            void* data) {
    auto& self = *static_cast<NetSocketImpl*>(data);
    self.callback_(
        {Notification::Kind::Close, self.generation_, {}, false, event->code});
    return true;
}

bool NetSocketImpl::onError(int, const EmscriptenWebSocketErrorEvent*,
                            void* data) {
    auto& self = *static_cast<NetSocketImpl*>(data);
    self.callback_({Notification::Kind::Error, self.generation_});
    return true;
}
#endif

}  // namespace ludork::global::net_impl
#endif
