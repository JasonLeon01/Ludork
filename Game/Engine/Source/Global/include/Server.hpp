#pragma once

#include <Runtime/Async/AsyncOperation.hpp>
#include <Runtime/StrictFunction.hpp>
#include <Server/ServerAccountResult.hpp>
#include <Server/ServerAccountListResult.hpp>
#include <Server/ServerFieldListResult.hpp>
#include <Server/ServerConnectionResult.hpp>
#include <Server/ServerReadResult.hpp>
#include <Server/ServerTokenResult.hpp>

/// Asynchronous access to the server configured when the project is packaged.
/// Callbacks run on the game logic thread after completing the operation with
/// the same result object. The default Entry enables these operation updates.
BIND_CLASS()
class LUDORK_GLOBAL_API Server {
public:
    using AccountListCallback = ludork::runtime::StrictFunction<void(
        std::shared_ptr<ServerAccountListResult>)>;
    using FieldListCallback = ludork::runtime::StrictFunction<void(
        std::shared_ptr<ServerFieldListResult>)>;
    using AccountCallback = ludork::runtime::StrictFunction<void(
        std::shared_ptr<ServerAccountResult>)>;
    using ReadCallback = ludork::runtime::StrictFunction<void(
        std::shared_ptr<ServerReadResult>)>;
    using TokenCallback = ludork::runtime::StrictFunction<void(
        std::shared_ptr<ServerTokenResult>)>;
    using WriteCallback =
        ludork::runtime::StrictFunction<void(std::shared_ptr<ServerResult>)>;
    using ConnectionCallback = ludork::runtime::StrictFunction<void(
        std::shared_ptr<ServerConnectionResult>)>;

    /// List accounts, optionally filtered by category. Nil or zero selects all;
    /// a positive integer samples distinct accounts. Pass nil for omissions.
    BIND_METHOD(nonnull_return = true)
    static std::shared_ptr<AsyncOperation> ListAccountsAsync(
        std::optional<std::int64_t> sampleCount,
        std::optional<std::string> categoryFilter,
        AccountListCallback onCompleted);

    /// List fields in descending UTF-8 byte order, strictly before beforeKey.
    /// Nil limit selects 50; explicit limits must be between 1 and 100.
    BIND_METHOD(nonnull_return = true)
    static std::shared_ptr<AsyncOperation> ListFieldsAsync(
        const std::string& accountId, const std::string& category,
        std::optional<std::string> beforeKey, std::optional<std::int64_t> limit,
        FieldListCallback onCompleted);

    /// Query account existence without creating or reserving the account.
    BIND_METHOD(nonnull_return = true)
    static std::shared_ptr<AsyncOperation> CheckAccountExistsAsync(
        const std::string& accountId, AccountCallback onCompleted);

    /// Read a field; found distinguishes an absent field from JSON null.
    BIND_METHOD(nonnull_return = true)
    static std::shared_ptr<AsyncOperation> ReadFieldAsync(
        const std::string& accountId, const std::string& category,
        const std::string& field, ReadCallback onCompleted);

    /// Acquire one account-scoped write capability with a 60-second lifetime.
    BIND_METHOD(nonnull_return = true)
    static std::shared_ptr<AsyncOperation> AcquireWriteTokenAsync(
        const std::string& accountId, TokenCallback onCompleted);

    /// Consume a capability once. A failed or timed-out write is never retried.
    BIND_METHOD(nonnull_return = true)
    static std::shared_ptr<AsyncOperation> WriteFieldAsync(
        std::shared_ptr<ServerWriteToken> token, const std::string& category,
        const std::string& field, const RuntimeData& value,
        WriteCallback onCompleted);

    /// Probe the configured service. Normal is at most 300 ms, Weak at most
    /// 3000 ms, and Disconnected means no valid health response in that time.
    BIND_METHOD(nonnull_return = true)
    static std::shared_ptr<AsyncOperation> CheckConnectionAsync(
        ConnectionCallback onCompleted);
};

namespace ludork::global {
void initializeServer();
void shutdownServer() noexcept;
}  // namespace ludork::global
