#pragma once

#include <GlobalRuntimeApi.hpp>
#include <Runtime/Async/AsyncOperation.hpp>
#include <Runtime/RuntimeIdentity.hpp>
#include <Runtime/StrictFunction.hpp>

#include <cstddef>
#include <string>

/// Native transport used by Global.Net. Callbacks run on the logic thread.
BIND_CLASS(metadata = false)
class LUDORK_GLOBAL_API NetTransport {
public:
    using Callback = ludork::runtime::StrictFunction<void(RuntimeIdentityPtr)>;
    using Transform =
        ludork::runtime::StrictFunction<RuntimeIdentityPtr(RuntimeIdentityPtr)>;

    /// Log a diagnostic and return false when server integration is disabled.
    BIND_METHOD(metadata = false)
    static bool CheckAvailable();

    /// Return null without invoking callbacks when server integration is
    /// disabled.
    BIND_METHOD(metadata = false, defaults = {nil}, allow_nil = "params")
    static std::shared_ptr<AsyncOperation> GetSnapshotAsync(
        const std::string& module, const RuntimeIdentityPtr& params,
        Transform transform, Callback onCompleted,
        std::shared_ptr<AsyncOperation> after = {});

    BIND_METHOD(metadata = false)
    static void CallRpcFunction(const std::string& module,
                                const std::string& name,
                                const RuntimeIdentityPtr& packedArguments);

    BIND_METHOD(metadata = false)
    static std::size_t Subscribe(const std::string& module,
                                 const std::string& event, Callback callback);

    BIND_METHOD(metadata = false)
    static std::size_t SubscribeError(const std::string& module,
                                      Callback callback);

    BIND_METHOD(metadata = false)
    static bool Unsubscribe(const std::string& module, std::size_t token);

    BIND_METHOD(metadata = false, allow_nil = "value")
    static RuntimeIdentityPtr CloneValue(const RuntimeIdentityPtr& value);
};

namespace ludork::global {
void initializeNetTransport();
void shutdownNetTransport() noexcept;
}  // namespace ludork::global
