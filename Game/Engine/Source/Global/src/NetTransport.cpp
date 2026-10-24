#include <LudorkGenerated/ServerConfig.hpp>
#include <NetTransport.hpp>

#include "Net/NetTransportImpl.hpp"
#include "Net/NetValueCodecImpl.hpp"

#include <iostream>

bool NetTransport::CheckAvailable() {
#if defined(LUDORK_SERVER_AVAILABLE)
    return true;
#else
    std::cerr
        << "Ludork Server is disabled for this project. Enable and configure "
           "the server in Test Server Settings or Pack Options.\n";
    return false;
#endif
}

std::shared_ptr<AsyncOperation> NetTransport::GetSnapshotAsync(
    const std::string& module, const RuntimeIdentityPtr& params,
    Transform transform, Callback onCompleted,
    std::shared_ptr<AsyncOperation> after) {
    if (!CheckAvailable()) {
        return {};
    }
    return ludork::global::net_impl::transport().snapshot(
        module, params, std::move(transform), std::move(onCompleted),
        std::move(after));
}

void NetTransport::CallRpcFunction(const std::string& module,
                                   const std::string& name,
                                   const RuntimeIdentityPtr& packedArguments) {
    if (!CheckAvailable()) {
        return;
    }
    ludork::global::net_impl::transport().callRpc(module, name,
                                                  packedArguments);
}

std::size_t NetTransport::Subscribe(const std::string& module,
                                    const std::string& event,
                                    Callback callback) {
    if (!CheckAvailable()) {
        return 0;
    }
    return ludork::global::net_impl::transport().subscribe(
        module, event, std::move(callback), false);
}

std::size_t NetTransport::SubscribeError(const std::string& module,
                                         Callback callback) {
    if (!CheckAvailable()) {
        return 0;
    }
    return ludork::global::net_impl::transport().subscribe(
        module, {}, std::move(callback), true);
}

bool NetTransport::Unsubscribe(const std::string& module, std::size_t token) {
    if (!CheckAvailable()) {
        return false;
    }
    return ludork::global::net_impl::transport().unsubscribe(module, token);
}

RuntimeIdentityPtr NetTransport::CloneValue(const RuntimeIdentityPtr& value) {
    ludork::global::net_impl::NetValueCodecImpl encoder;
    const RuntimeData encoded = encoder.encode(value);
    ludork::global::net_impl::NetValueCodecImpl decoder;
    return decoder.decode(encoded);
}
