#include <NetTransport.hpp>

#include "Net/NetTransportImpl.hpp"
#include "Net/NetValueCodecImpl.hpp"

std::shared_ptr<AsyncOperation> NetTransport::GetSnapshotAsync(
    const std::string& module, const RuntimeIdentityPtr& params,
    Transform transform, Callback onCompleted,
    std::shared_ptr<AsyncOperation> after) {
    return ludork::global::net_impl::transport().snapshot(
        module, params, std::move(transform), std::move(onCompleted),
        std::move(after));
}

void NetTransport::CallRpcFunction(const std::string& module,
                                   const std::string& name,
                                   const RuntimeIdentityPtr& packedArguments) {
    ludork::global::net_impl::transport().callRpc(module, name,
                                                  packedArguments);
}

std::size_t NetTransport::Subscribe(const std::string& module,
                                    const std::string& event,
                                    Callback callback) {
    return ludork::global::net_impl::transport().subscribe(
        module, event, std::move(callback), false);
}

std::size_t NetTransport::SubscribeError(const std::string& module,
                                         Callback callback) {
    return ludork::global::net_impl::transport().subscribe(
        module, {}, std::move(callback), true);
}

bool NetTransport::Unsubscribe(const std::string& module, std::size_t token) {
    return ludork::global::net_impl::transport().unsubscribe(module, token);
}

RuntimeIdentityPtr NetTransport::CloneValue(const RuntimeIdentityPtr& value) {
    ludork::global::net_impl::NetValueCodecImpl encoder;
    const RuntimeData encoded = encoder.encode(value);
    ludork::global::net_impl::NetValueCodecImpl decoder;
    return decoder.decode(encoded);
}
