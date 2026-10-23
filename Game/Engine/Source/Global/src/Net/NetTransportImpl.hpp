#pragma once

#include <LudorkGenerated/ServerConfig.hpp>
#include <NetTransport.hpp>
#include <Runtime/RuntimeData.hpp>
#include <Utils/EventBus.hpp>

#if defined(LUDORK_SERVER_AVAILABLE)
#include <ixwebsocket/IXWebSocket.h>
#endif

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ludork::global::net_impl {

class NetTransportImpl {
public:
    NetTransportImpl();
    ~NetTransportImpl();

    std::shared_ptr<AsyncOperation> snapshot(
        const std::string& module, const RuntimeIdentityPtr& params,
        NetTransport::Transform transform, NetTransport::Callback callback,
        std::shared_ptr<AsyncOperation> after);
    void callRpc(const std::string& module, const std::string& name,
                 const RuntimeIdentityPtr& arguments);
    std::size_t subscribe(const std::string& module, const std::string& event,
                          NetTransport::Callback callback, bool error);
    bool unsubscribe(const std::string& module, std::size_t token);

private:
    using Clock = std::chrono::steady_clock;

    struct Pending {
        std::string id;
        std::string module;
        std::string name;
        std::string bytes;
        std::string errorCode;
        std::string errorMessage;
        Clock::time_point deadline = Clock::now() + std::chrono::seconds(10);
        std::uint64_t generation = 0;
        bool sent = false;
        std::shared_ptr<AsyncOperation> operation;
        std::shared_ptr<AsyncOperation> after;
        NetTransport::Transform transform;
        NetTransport::Callback callback;
    };

    struct Notification {
        enum class Kind {
            Open,
            Message,
            Close,
            Error
        };
        Kind kind = Kind::Message;
        std::uint64_t generation = 0;
        std::string bytes;
        bool binary = false;
        int status = 0;
    };

    static void validateName(const std::string& name);
    static RuntimeIdentityPtr result(bool ok, const std::string& code,
                                     const std::string& message,
                                     const std::string& requestId = {});
    static void completeSnapshot(Pending& pending, AsyncOperation& operation,
                                 const RuntimeIdentityPtr& value);
    static std::shared_ptr<AsyncOperation> rejected(
        const std::shared_ptr<Pending>& pending);
    EventBus& bus(const std::string& module);
    void ensureStarted();
    void startConnection();
    bool enqueue(const std::shared_ptr<Pending>& pending,
                 RuntimeData::Map request);
    void poll();
    std::function<void()> process(const Notification& notification);
    std::function<void()> processMessage(const std::string& bytes);
    void flushOutgoing();
    void settle(const std::shared_ptr<Pending>& pending,
                const RuntimeIdentityPtr& value);
    void failAll(const std::string& code, const std::string& message);
    void connectionFailure(const std::string& code, const std::string& message);
    void emitError(const std::string& module, const std::string& name,
                   const RuntimeIdentityPtr& value);
    void remove(const std::shared_ptr<Pending>& pending);
    void queueNotification(Notification notification);

#if defined(LUDORK_SERVER_AVAILABLE)
    std::unique_ptr<ix::WebSocket> socket_;
#else
    std::vector<std::string> disabledModules_;
#endif
    std::shared_ptr<AsyncOperation> polling_;
    std::unordered_map<std::string, std::unique_ptr<EventBus>> buses_;
    std::deque<std::shared_ptr<Pending>> pending_;
    std::size_t pendingBytes_ = 0;
    std::uint64_t nextId_ = 0;
    std::uint64_t generation_ = 0;
    Clock::time_point handshakeDeadline_;
    Clock::time_point reconnectAt_;
    bool started_ = false;
    bool ready_ = false;
    bool stopping_ = false;
    bool authenticationFailed_ = false;
    bool reconnectPending_ = false;
    std::mutex inboundMutex_;
    std::deque<Notification> inbound_;
    std::size_t inboundBytes_ = 0;
    bool inboundOverflow_ = false;
};

NetTransportImpl& transport();

}  // namespace ludork::global::net_impl
