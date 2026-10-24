#include "NetTransportImpl.hpp"
#include "NetValueCodecImpl.hpp"

#include <Runtime/Json.hpp>
#include <Runtime/RuntimeDataReader.hpp>
#include <Runtime/RuntimeReference.hpp>

#if defined(LUDORK_SERVER_AVAILABLE)
#include <ixwebsocket/IXNetSystem.h>
#endif

#include <algorithm>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ludork::global::net_impl {
namespace {

constexpr std::size_t MaxQueueCount = 256;
constexpr std::size_t MaxQueueBytes = 8 * 1024 * 1024;
constexpr const char* ErrorEvent = "error";
std::unique_ptr<NetTransportImpl> instance;

template <typename Callback>
void captureFailure(std::exception_ptr& failure, Callback&& callback) {
    try {
        std::forward<Callback>(callback)();
    } catch (...) {
        if (!failure) {
            failure = std::current_exception();
        }
    }
}

const RuntimeData& field(const RuntimeData::Map& object, const char* key) {
    return ludork::runtime::value_reader::requireValue(object, key,
                                                       "Network message");
}

std::string textField(const RuntimeData::Map& object, const char* key) {
    return ludork::runtime::value_reader::requireString(field(object, key),
                                                        "Network message");
}

void validateJsonDepth(const std::string& bytes) {
    std::size_t depth = 0;
    bool quoted = false;
    bool escaped = false;
    for (const char byte : bytes) {
        if (quoted) {
            if (escaped) {
                escaped = false;
            } else if (byte == '\\') {
                escaped = true;
            } else if (byte == '"') {
                quoted = false;
            }
        } else if (byte == '"') {
            quoted = true;
        } else if (byte == '[' || byte == '{') {
            if (++depth > NetValueCodecImpl::MaxDepth * 3 + 16) {
                throw std::invalid_argument(
                    "Network JSON nesting exceeds its limit.");
            }
        } else if (byte == ']' || byte == '}') {
            if (depth == 0) {
                throw std::invalid_argument("Invalid network JSON nesting.");
            }
            --depth;
        }
    }
}

}  // namespace

NetTransportImpl::NetTransportImpl() {
#if defined(LUDORK_SERVER_AVAILABLE)
    if (!ix::initNetSystem()) {
        throw std::runtime_error("Could not initialize the network transport.");
    }
#endif
}

NetTransportImpl::~NetTransportImpl() {
    stopping_ = true;
    if (polling_) {
        polling_->cancel();
    }
#if defined(LUDORK_SERVER_AVAILABLE)
    if (socket_) {
        socket_->stop();
        socket_.reset();
    }
#endif
    for (const std::shared_ptr<Pending>& pending : pending_) {
        if (pending->operation) {
            pending->operation->cancel();
        }
    }
    pending_.clear();
    buses_.clear();
#if defined(LUDORK_SERVER_AVAILABLE)
    ix::uninitNetSystem();
#endif
}

void NetTransportImpl::validateName(const std::string& name) {
    if (name.empty() || name.size() > 128 ||
        !std::all_of(name.begin(), name.end(), [](unsigned char byte) {
            return (byte >= 'a' && byte <= 'z') ||
                   (byte >= 'A' && byte <= 'Z') ||
                   (byte >= '0' && byte <= '9') || byte == '_' || byte == '.' ||
                   byte == '-';
        })) {
        throw std::invalid_argument(
            "Network names require 1 to 128 ASCII letters, digits, dots, "
            "underscores or hyphens.");
    }
}

RuntimeIdentityPtr NetTransportImpl::result(bool ok, const std::string& code,
                                            const std::string& message,
                                            const std::string& requestId) {
    const RuntimeHandle value = ludork::runtime::reference::table();
    ludork::runtime::reference::rawSet(value, "ok", ok);
    ludork::runtime::reference::rawSet(value, "code", code);
    ludork::runtime::reference::rawSet(value, "message", message);
    if (!requestId.empty()) {
        ludork::runtime::reference::rawSet(value, "requestId", requestId);
    }
    return value.identity();
}

void NetTransportImpl::completeSnapshot(Pending& pending,
                                        AsyncOperation& operation,
                                        const RuntimeIdentityPtr& value) {
    if (!operation.isPending()) {
        return;
    }
    RuntimeIdentityPtr transformed;
    try {
        transformed = pending.transform(value);
    } catch (...) {
        operation.cancel();
        throw;
    }
    operation.complete(RuntimeValue(RuntimeHandle(transformed)));
    pending.callback(transformed);
}

std::shared_ptr<AsyncOperation> NetTransportImpl::rejected(
    const std::shared_ptr<Pending>& pending) {
    return AsyncOperation::create([pending](AsyncOperation& operation) {
        completeSnapshot(*pending, operation,
                         result(false, pending->errorCode,
                                pending->errorMessage, pending->id));
    });
}

EventBus& NetTransportImpl::bus(const std::string& module) {
    std::unique_ptr<EventBus>& value = buses_[module];
    if (!value) {
        value = std::make_unique<EventBus>();
    }
    return *value;
}

void NetTransportImpl::ensureStarted() {
    if (stopping_) {
        throw std::logic_error("The network session is stopping.");
    }
    if (!polling_) {
        polling_ = AsyncOperation::create([this](AsyncOperation&) {
            poll();
        });
    }
    if (started_) {
        return;
    }
    started_ = true;
    startConnection();
}

void NetTransportImpl::startConnection() {
#if defined(LUDORK_SERVER_AVAILABLE)
    std::string url = ludork::generated::server::Url;
    if (!url.starts_with("http://")) {
        throw std::invalid_argument("The compiled server URL is invalid.");
    }
    while (url.ends_with('/')) {
        url.pop_back();
    }
    socket_ = std::make_unique<ix::WebSocket>();
    socket_->setUrl("ws://" + url.substr(7) + "/api/v1/net");
    socket_->setExtraHeaders(
        {{"X-Ludork-Key", ludork::generated::server::Key}});
    socket_->disablePerMessageDeflate();
    socket_->disableAutomaticReconnection();
    socket_->setHandshakeTimeout(10);
    socket_->setPingInterval(15);
    const std::uint64_t generation = ++generation_;
    socket_->setOnMessageCallback(
        [this, generation](const ix::WebSocketMessagePtr& message) {
            Notification notification;
            notification.generation = generation;
            if (message->type == ix::WebSocketMessageType::Open) {
                notification.kind = Notification::Kind::Open;
            } else if (message->type == ix::WebSocketMessageType::Message) {
                notification.kind = Notification::Kind::Message;
                notification.binary = message->binary;
                if (message->str.size() > NetValueCodecImpl::MaxMessageBytes) {
                    std::lock_guard lock(inboundMutex_);
                    inboundOverflow_ = true;
                    return;
                }
                notification.bytes = message->str;
            } else if (message->type == ix::WebSocketMessageType::Close) {
                notification.kind = Notification::Kind::Close;
            } else if (message->type == ix::WebSocketMessageType::Error) {
                notification.kind = Notification::Kind::Error;
                notification.status = message->errorInfo.http_status;
            } else {
                return;
            }
            queueNotification(std::move(notification));
        });
    reconnectPending_ = false;
    handshakeDeadline_ = Clock::now() + std::chrono::seconds(10);
    socket_->start();
#endif
}

bool NetTransportImpl::enqueue(const std::shared_ptr<Pending>& pending,
                               RuntimeData::Map request) {
    ensureStarted();
    pending->id = std::to_string(++nextId_);
    request.emplace("id", RuntimeData(pending->id));
    pending->bytes = stringifyJSON(RuntimeData(std::move(request)));
    if (pending->bytes.size() > NetValueCodecImpl::MaxMessageBytes) {
        pending->errorCode = "InvalidArgument";
        pending->errorMessage = "The network request exceeds 2 MiB.";
    } else if (authenticationFailed_) {
        pending->errorCode = "AuthenticationFailed";
        pending->errorMessage = "The server rejected the project key.";
    } else if (pending_.size() >= MaxQueueCount ||
               pending->bytes.size() > MaxQueueBytes - pendingBytes_) {
        pending->errorCode = "QueueFull";
        pending->errorMessage = "The network request queue is full.";
    }
    if (!pending->errorCode.empty()) {
        return false;
    }
    pendingBytes_ += pending->bytes.size();
    pending_.push_back(pending);
    return true;
}

std::shared_ptr<AsyncOperation> NetTransportImpl::snapshot(
    const std::string& module, const RuntimeIdentityPtr& params,
    NetTransport::Transform transform, NetTransport::Callback callback,
    std::shared_ptr<AsyncOperation> after) {
    validateName(module);
    if (!transform || !callback) {
        throw std::invalid_argument(
            "A snapshot requires transform and completion callbacks.");
    }
    NetValueCodecImpl codec;
    RuntimeData encoded = codec.encode(params);
    const auto pending = std::make_shared<Pending>();
    pending->module = module;
    pending->transform = std::move(transform);
    pending->callback = std::move(callback);
    pending->after = std::move(after);
    if (!enqueue(pending, {{"type", RuntimeData("snapshot")},
                           {"module", RuntimeData(module)},
                           {"params", std::move(encoded)}})) {
        return rejected(pending);
    }
    pending->operation = AsyncOperation::create();
    return pending->operation;
}

void NetTransportImpl::callRpc(const std::string& module,
                               const std::string& name,
                               const RuntimeIdentityPtr& arguments) {
    validateName(module);
    validateName(name);
    NetValueCodecImpl codec;
    RuntimeData::Array encoded = codec.encodeArguments(arguments);
    const auto pending = std::make_shared<Pending>();
    pending->module = module;
    pending->name = name;
    if (!enqueue(pending, {{"type", RuntimeData("rpc")},
                           {"module", RuntimeData(module)},
                           {"name", RuntimeData(name)},
                           {"args", RuntimeData(std::move(encoded))}})) {
        emitError(module, name,
                  result(false, pending->errorCode, pending->errorMessage,
                         pending->id));
    }
}

std::size_t NetTransportImpl::subscribe(const std::string& module,
                                        const std::string& event,
                                        NetTransport::Callback callback,
                                        bool error) {
    validateName(module);
    if (!error) {
        validateName(event);
    }
    if (!callback) {
        throw std::invalid_argument(
            "A network subscription requires a callback.");
    }
    const std::size_t token = bus(module).subscribe(
        error ? ErrorEvent : "event:" + event,
        [callback = std::move(callback)](const RuntimeValue& value) {
            callback(ludork::runtime::reference::intern(value).identity());
        });
    ensureStarted();
    return token;
}

bool NetTransportImpl::unsubscribe(const std::string& module,
                                   std::size_t token) {
    const auto found = buses_.find(module);
    return found != buses_.end() && found->second->unsubscribe(token);
}

void NetTransportImpl::queueNotification(Notification notification) {
    std::lock_guard lock(inboundMutex_);
    if (inbound_.size() >= MaxQueueCount ||
        notification.bytes.size() > MaxQueueBytes - inboundBytes_) {
        inboundOverflow_ = true;
        return;
    }
    inboundBytes_ += notification.bytes.size();
    inbound_.push_back(std::move(notification));
}

void NetTransportImpl::remove(const std::shared_ptr<Pending>& pending) {
    const auto found = std::find(pending_.begin(), pending_.end(), pending);
    if (found != pending_.end()) {
        pendingBytes_ -= pending->bytes.size();
        pending_.erase(found);
    }
}

void NetTransportImpl::emitError(const std::string& module,
                                 const std::string& name,
                                 const RuntimeIdentityPtr& value) {
    const RuntimeHandle error(value);
    ludork::runtime::reference::rawSet(error, "module", module);
    if (!name.empty()) {
        ludork::runtime::reference::rawSet(error, "functionName", name);
    }
    const std::string code = ludork::runtime::reference::as<std::string>(
        ludork::runtime::reference::rawGet(error, "code"));
    const std::string message = ludork::runtime::reference::as<std::string>(
        ludork::runtime::reference::rawGet(error, "message"));
    std::cerr << "ERROR:Network " << module;
    if (!name.empty()) {
        std::cerr << '.' << name;
    }
    std::cerr << " failed (" << code << "): " << message << '\n';
    bus(module).publish(ErrorEvent, RuntimeValue(error));
}

void NetTransportImpl::settle(const std::shared_ptr<Pending>& pending,
                              const RuntimeIdentityPtr& value) {
    remove(pending);
    ludork::runtime::reference::rawSet(RuntimeHandle(value), "requestId",
                                       pending->id);
    if (pending->operation) {
        completeSnapshot(*pending, *pending->operation, value);
    } else if (!ludork::runtime::reference::boolean(
                   ludork::runtime::reference::rawGet(RuntimeHandle(value),
                                                      "ok"))) {
        emitError(pending->module, pending->name, value);
    }
}

void NetTransportImpl::failAll(const std::string& code,
                               const std::string& message) {
    const std::deque<std::shared_ptr<Pending>> pending = std::move(pending_);
    pending_.clear();
    pendingBytes_ = 0;
    std::exception_ptr failure;
    for (const std::shared_ptr<Pending>& request : pending) {
        captureFailure(failure, [&] {
            settle(request, result(false, code, message));
        });
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

void NetTransportImpl::connectionFailure(const std::string& code,
                                         const std::string& message) {
    if (reconnectPending_) {
        return;
    }
    ready_ = false;
    reconnectPending_ = true;
    reconnectAt_ = Clock::now() + std::chrono::seconds(1);
#if defined(LUDORK_SERVER_AVAILABLE)
    if (socket_ && socket_->getReadyState() == ix::ReadyState::Open) {
        socket_->close();
    }
#endif
    std::exception_ptr failure;
    captureFailure(failure, [&] {
        failAll(code, message);
    });
    std::vector<std::string> modules;
    modules.reserve(buses_.size());
    for (const auto& [module, eventBus] : buses_) {
        static_cast<void>(eventBus);
        modules.push_back(module);
    }
    for (const std::string& module : modules) {
        captureFailure(failure, [&] {
            emitError(module, {}, result(false, code, message));
        });
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

std::function<void()> NetTransportImpl::process(
    const Notification& notification) {
    if (notification.generation < generation_) {
        return {};
    }
    if (notification.kind == Notification::Kind::Open) {
        generation_ = notification.generation;
        ready_ = false;
        handshakeDeadline_ = Clock::now() + std::chrono::seconds(10);
    } else if (notification.kind == Notification::Kind::Message) {
        if (notification.generation != generation_ || notification.binary) {
            throw std::invalid_argument(
                "Invalid network message generation or encoding.");
        }
        return processMessage(notification.bytes);
    } else if (notification.kind == Notification::Kind::Error &&
               (notification.status == 401 || notification.status == 403)) {
        authenticationFailed_ = true;
        return [this] {
            connectionFailure("AuthenticationFailed",
                              "The server rejected the project key.");
        };
    } else {
        return [this] {
            connectionFailure("ConnectionFailed",
                              "The server connection was lost.");
        };
    }
    return {};
}

std::function<void()> NetTransportImpl::processMessage(
    const std::string& bytes) {
    validateJsonDepth(bytes);
    const RuntimeData decoded = parseJSONText(bytes);
    const RuntimeData::Map& message =
        ludork::runtime::value_reader::requireMap(decoded, "Network message");
    const std::string type = textField(message, "type");
    if (type == "ready") {
        if (ready_ || ludork::runtime::value_reader::requireInteger(
                          field(message, "version"), "Network version") != 1) {
            throw std::invalid_argument(
                "Unsupported network protocol version.");
        }
        ready_ = true;
        return {};
    }
    if (!ready_) {
        throw std::invalid_argument("Network message received before ready.");
    }
    if (type == "event") {
        const std::string module = textField(message, "module");
        const std::string event = textField(message, "event");
        validateName(module);
        validateName(event);
        NetValueCodecImpl codec;
        const RuntimeIdentityPtr arguments =
            codec.decodeArguments(ludork::runtime::value_reader::requireArray(
                field(message, "args"), "Network arguments"));
        return [this, module, event, arguments] {
            const auto found = buses_.find(module);
            if (found != buses_.end()) {
                found->second->publish("event:" + event,
                                       RuntimeValue(RuntimeHandle(arguments)));
            }
        };
    }
    if (type != "result") {
        throw std::invalid_argument("Unknown network message type.");
    }
    const std::string id = textField(message, "id");
    const auto found =
        std::find_if(pending_.begin(), pending_.end(),
                     [&id](const std::shared_ptr<Pending>& request) {
                         return request->id == id;
                     });
    if (found == pending_.end()) {
        return {};
    }
    const std::shared_ptr<Pending> pending = *found;
    if (!pending->sent || pending->generation != generation_) {
        throw std::invalid_argument("Unexpected network response.");
    }
    if (Clock::now() >= pending->deadline) {
        return [this, pending] {
            settle(pending,
                   result(false, "Timeout", "The server request timed out."));
        };
    }
    const bool ok = ludork::runtime::value_reader::requireBool(
        field(message, "ok"), "Network response");
    const RuntimeIdentityPtr value =
        result(ok, ok ? "None" : textField(message, "code"),
               ok ? "" : textField(message, "message"));
    if (ok && pending->operation) {
        NetValueCodecImpl codec;
        const RuntimeIdentityPtr data = codec.decode(field(message, "data"));
        if (!ludork::runtime::reference::isTable(RuntimeValue(data))) {
            throw std::invalid_argument(
                "Network snapshots require an ordinary table.");
        }
        ludork::runtime::reference::rawSet(RuntimeHandle(value), "data",
                                           RuntimeValue(data));
    } else if (ok && message.contains("data")) {
        throw std::invalid_argument("RPC functions cannot return data.");
    }
    if (message.contains("retryAfterSeconds")) {
        const std::int64_t retry =
            ludork::runtime::value_reader::requireInteger(
                field(message, "retryAfterSeconds"), "Network retry interval");
        if (ok || retry < 1) {
            throw std::invalid_argument("Invalid network retry interval.");
        }
        ludork::runtime::reference::rawSet(RuntimeHandle(value),
                                           "retryAfterSeconds", retry);
    }
    return [this, pending, value] {
        settle(pending, value);
    };
}

void NetTransportImpl::flushOutgoing() {
#if defined(LUDORK_SERVER_AVAILABLE)
    if (!ready_ || !socket_ ||
        socket_->getReadyState() != ix::ReadyState::Open) {
        return;
    }
    for (const std::shared_ptr<Pending>& pending : pending_) {
        if (pending->sent) {
            continue;
        }
        if (pending->after && pending->after->isPending()) {
            break;
        }
        pending->after.reset();
        if (socket_->bufferedAmount() > MaxQueueBytes - pending->bytes.size()) {
            break;
        }
        pending->sent = true;
        pending->generation = generation_;
        if (!socket_->sendText(pending->bytes).success) {
            connectionFailure("ConnectionFailed",
                              "The server request could not be sent.");
            return;
        }
    }
#endif
}

void NetTransportImpl::poll() {
    std::exception_ptr failure;
    std::deque<Notification> notifications;
    bool overflow = false;
    {
        std::lock_guard lock(inboundMutex_);
        notifications.swap(inbound_);
        inboundBytes_ = 0;
        overflow = std::exchange(inboundOverflow_, false);
    }
    if (overflow) {
        captureFailure(failure, [&] {
            connectionFailure(
                "QueueFull",
                "The network receive queue or message exceeded its limit.");
        });
#if defined(LUDORK_SERVER_AVAILABLE)
        if (socket_) {
            socket_->close(1009, "Message limit exceeded");
        }
#endif
        notifications.clear();
    }
    for (const Notification& notification : notifications) {
        std::function<void()> dispatch;
        try {
            dispatch = process(notification);
        } catch (const std::exception&) {
            captureFailure(failure, [&] {
                connectionFailure(
                    "InvalidResponse",
                    "The server returned an invalid network message.");
            });
#if defined(LUDORK_SERVER_AVAILABLE)
            if (socket_) {
                socket_->close(1002, "Invalid network message");
            }
#endif
            break;
        }
        if (dispatch) {
            captureFailure(failure, dispatch);
        }
    }
    const std::deque<std::shared_ptr<Pending>> pending = pending_;
    for (const std::shared_ptr<Pending>& request : pending) {
        if (request->operation && !request->operation->isPending()) {
            remove(request);
        } else if (Clock::now() >= request->deadline) {
            captureFailure(failure, [&] {
                settle(request, result(false, "Timeout",
                                       "The server request timed out."));
            });
        }
    }
#if defined(LUDORK_SERVER_AVAILABLE)
    if (!ready_ && socket_ &&
        socket_->getReadyState() == ix::ReadyState::Open &&
        Clock::now() >= handshakeDeadline_) {
        captureFailure(failure, [&] {
            connectionFailure("Timeout",
                              "The server protocol handshake timed out.");
        });
        socket_->close(1002, "Protocol handshake timed out");
    }
    if (reconnectPending_ && socket_ &&
        socket_->getReadyState() == ix::ReadyState::Closed) {
        socket_->stop();
        socket_.reset();
    }
    if (!socket_ && !authenticationFailed_ && Clock::now() >= reconnectAt_) {
        startConnection();
    }
#endif
    captureFailure(failure, [this] {
        flushOutgoing();
    });
    if (failure) {
        std::rethrow_exception(failure);
    }
}

NetTransportImpl& transport() {
    if (!instance) {
        throw std::logic_error(
            "Network transport requires a running game session.");
    }
    return *instance;
}

}  // namespace ludork::global::net_impl

namespace ludork::global {

void initializeNetTransport() {
    if (!net_impl::instance) {
        net_impl::instance = std::make_unique<net_impl::NetTransportImpl>();
    }
}

void shutdownNetTransport() noexcept {
    net_impl::instance.reset();
}

}  // namespace ludork::global
