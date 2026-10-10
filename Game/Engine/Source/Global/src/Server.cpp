#include <LudorkGenerated/ServerConfig.hpp>
#include <Server.hpp>

#include "Server/ServerRequestData.hpp"
#if defined(LUDORK_SERVER_AVAILABLE)
#include "Server/ServerTransportImpl.hpp"
#endif

#include <Runtime/Json.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace {
using Request = ludork::global::server_impl::Request;
using Response = ludork::global::server_impl::Response;
using PendingRequest = ludork::global::server_impl::PendingRequest;

Response invalidArgument() {
    return Response{
        ServerErrorCode::InvalidArgument,
        "Account, category and field must contain 1 to 64 UTF-8 "
        "bytes without control characters; . and .. are not valid names."};
}

Response invalidResponse() {
    return Response{ServerErrorCode::InvalidResponse,
                    "The server returned an invalid response."};
}

std::string accountPath(const std::string& account) {
    return "/accounts/" + ludork::global::server_impl::encodeSegment(account);
}

std::string fieldsPath(const std::string& account,
                       const std::string& category) {
    return accountPath(account) + "/categories/" +
           ludork::global::server_impl::encodeSegment(category) + "/fields";
}

std::string fieldPath(const std::string& account, const std::string& category,
                      const std::string& field) {
    return fieldsPath(account, category) + "/" +
           ludork::global::server_impl::encodeSegment(field);
}

void appendQuery(std::string& path, const std::string& name,
                 const std::string& value) {
    path += path.find('?') == std::string::npos ? '?' : '&';
    path += name + "=" + ludork::global::server_impl::encodeSegment(value);
}

bool byteLess(const std::string& left, const std::string& right) {
    return std::lexicographical_compare(
        left.begin(), left.end(), right.begin(), right.end(),
        [](unsigned char first, unsigned char second) {
            return first < second;
        });
}

template <typename Result, typename Callback, typename Factory>
std::shared_ptr<AsyncOperation> schedule(
    Request request, Callback callback, Factory factory,
    std::optional<Response> immediate = {}) {
    if (!callback) {
        throw std::invalid_argument(
            "Server operation requires a completion callback.");
    }
    const auto pending = std::make_shared<PendingRequest>();
    if (request.health) {
        pending->deadline = pending->started + std::chrono::seconds(3);
    }
    pending->request = std::move(request);
#if defined(LUDORK_SERVER_AVAILABLE)
    pending->response = std::move(immediate);
#else
    static_cast<void>(immediate);
    static bool reported = false;
    if (!reported) {
        reported = true;
        std::cerr << "Ludork Server is disabled for this project. Enable "
                     "Ludork Server when packaging the project.\n";
    }
    pending->response = Response{ServerErrorCode::Disabled,
                                 "Ludork Server is disabled for this project."};
#endif
    const auto operation = AsyncOperation::create(
        [pending, callback = std::move(callback),
         factory = std::move(factory)](AsyncOperation& current) {
            std::optional<Response> response;
            {
                std::lock_guard lock(pending->mutex);
                if (pending->response) {
                    response = std::move(pending->response);
                    pending->response.reset();
                }
            }
            if (!response &&
                PendingRequest::Clock::now() >= pending->deadline) {
                pending->cancelled.store(true);
                response = Response{ServerErrorCode::Timeout,
                                    "The server request timed out."};
            }
            if (!response) {
                return;
            }
            const std::shared_ptr<Result> result =
                factory(std::move(*response));
            current.complete(
                RuntimeValue(std::static_pointer_cast<RuntimeObject>(result)));
            callback(result);
        });
    operation->onCancelled([pending] {
        pending->cancelled.store(true);
    });
#if defined(LUDORK_SERVER_AVAILABLE)
    if (!pending->response) {
        ludork::global::server_impl::transport().enqueue(pending);
    }
#endif
    return operation;
}
}  // namespace

std::shared_ptr<AsyncOperation> Server::ListAccountsAsync(
    std::optional<std::int64_t> sampleCount,
    std::optional<std::string> categoryFilter,
    AccountListCallback onCompleted) {
    std::optional<Response> error;
    const auto count = sampleCount.value_or(0);
    if (count < 0) {
        error = Response{ServerErrorCode::InvalidArgument,
                         "Sample count must be a nonnegative integer."};
    } else if (categoryFilter &&
               !ludork::global::server_impl::validIdentifier(*categoryFilter)) {
        error = invalidArgument();
    }
    Request request{"GET", "/accounts"};
    if (sampleCount) {
        appendQuery(request.path, "sampleCount", std::to_string(count));
    }
    if (categoryFilter) {
        appendQuery(request.path, "category", *categoryFilter);
    }
    return schedule<ServerAccountListResult>(
        std::move(request), std::move(onCompleted),
        [count](Response response) {
            std::vector<std::string> accounts;
            if (response.code == ServerErrorCode::None) {
                const auto* body = response.body.getIf<RuntimeData::Map>();
                const auto* items =
                    body != nullptr && body->contains("accounts")
                        ? body->at("accounts").getIf<RuntimeData::Array>()
                        : nullptr;
                bool valid =
                    items != nullptr &&
                    (count == 0 ||
                     items->size() <= static_cast<std::uint64_t>(count));
                std::unordered_set<std::string> seen;
                if (valid) {
                    for (const auto& item : *items) {
                        const auto* account = item.getIf<std::string>();
                        if (account == nullptr ||
                            !ludork::global::server_impl::validIdentifier(
                                *account) ||
                            !seen.insert(*account).second ||
                            (count == 0 && !accounts.empty() &&
                             !byteLess(accounts.back(), *account))) {
                            valid = false;
                            break;
                        }
                        accounts.push_back(*account);
                    }
                }
                if (!valid) {
                    response = invalidResponse();
                    accounts.clear();
                }
            }
            return std::make_shared<ServerAccountListResult>(
                response.code, std::move(response.message),
                std::move(accounts));
        },
        std::move(error));
}

std::shared_ptr<AsyncOperation> Server::ListFieldsAsync(
    const std::string& accountId, const std::string& category,
    std::optional<std::string> beforeKey, std::optional<std::int64_t> limit,
    FieldListCallback onCompleted) {
    std::optional<Response> error;
    const auto pageSize = limit.value_or(50);
    if (!ludork::global::server_impl::validIdentifier(accountId) ||
        !ludork::global::server_impl::validIdentifier(category) ||
        (beforeKey &&
         !ludork::global::server_impl::validIdentifier(*beforeKey))) {
        error = invalidArgument();
    } else if (pageSize < 1 || pageSize > 100) {
        error = Response{ServerErrorCode::InvalidArgument,
                         "Limit must be an integer between 1 and 100."};
    }
    Request request{"GET", fieldsPath(accountId, category)};
    if (beforeKey) {
        appendQuery(request.path, "before", *beforeKey);
    }
    if (limit) {
        appendQuery(request.path, "limit", std::to_string(pageSize));
    }
    return schedule<ServerFieldListResult>(
        std::move(request), std::move(onCompleted),
        [beforeKey = std::move(beforeKey), pageSize](Response response) {
            std::vector<ServerFieldEntry> entries;
            std::optional<std::string> cursor;
            if (response.code == ServerErrorCode::None) {
                const auto* body = response.body.getIf<RuntimeData::Map>();
                const auto* items =
                    body != nullptr && body->contains("entries")
                        ? body->at("entries").getIf<RuntimeData::Array>()
                        : nullptr;
                bool valid =
                    items != nullptr &&
                    items->size() <= static_cast<std::uint64_t>(pageSize) &&
                    body->contains("nextCursor");
                if (valid) {
                    const auto& next = body->at("nextCursor");
                    if (!next.isNil()) {
                        const auto* key = next.getIf<std::string>();
                        valid =
                            key != nullptr &&
                            ludork::global::server_impl::validIdentifier(*key);
                        if (valid) {
                            cursor = *key;
                        }
                    }
                }
                if (valid) {
                    for (const auto& item : *items) {
                        const auto* entry = item.getIf<RuntimeData::Map>();
                        const auto* key =
                            entry != nullptr && entry->contains("key")
                                ? entry->at("key").getIf<std::string>()
                                : nullptr;
                        if (key == nullptr || !entry->contains("value") ||
                            !ludork::global::server_impl::validIdentifier(
                                *key) ||
                            (beforeKey && !byteLess(*key, *beforeKey)) ||
                            (!entries.empty() &&
                             !byteLess(*key, entries.back().getKey()))) {
                            valid = false;
                            break;
                        }
                        entries.emplace_back(*key, entry->at("value"));
                    }
                }
                if (valid && cursor) {
                    valid = entries.size() ==
                                static_cast<std::uint64_t>(pageSize) &&
                            !entries.empty() &&
                            *cursor == entries.back().getKey();
                }
                if (!valid) {
                    response = invalidResponse();
                    entries.clear();
                    cursor.reset();
                }
            }
            return std::make_shared<ServerFieldListResult>(
                response.code, std::move(response.message), std::move(entries),
                std::move(cursor));
        },
        std::move(error));
}

std::shared_ptr<AsyncOperation> Server::CheckAccountExistsAsync(
    const std::string& accountId, AccountCallback onCompleted) {
    std::optional<Response> error;
    if (!ludork::global::server_impl::validIdentifier(accountId)) {
        error = invalidArgument();
    }
    Request request{"GET", accountPath(accountId)};
    return schedule<ServerAccountResult>(
        std::move(request), std::move(onCompleted),
        [](Response response) {
            bool exists = false;
            if (response.code == ServerErrorCode::None) {
                const auto* body = response.body.getIf<RuntimeData::Map>();
                if (body == nullptr || !body->contains("exists") ||
                    body->at("exists").getIf<bool>() == nullptr) {
                    response = invalidResponse();
                } else {
                    exists = *body->at("exists").getIf<bool>();
                }
            }
            return std::make_shared<ServerAccountResult>(
                response.code, std::move(response.message), exists);
        },
        std::move(error));
}

std::shared_ptr<AsyncOperation> Server::ReadFieldAsync(
    const std::string& accountId, const std::string& category,
    const std::string& field, ReadCallback onCompleted) {
    std::optional<Response> error;
    if (!ludork::global::server_impl::validIdentifier(accountId) ||
        !ludork::global::server_impl::validIdentifier(category) ||
        !ludork::global::server_impl::validIdentifier(field)) {
        error = invalidArgument();
    }
    Request request{"GET", fieldPath(accountId, category, field)};
    return schedule<ServerReadResult>(
        std::move(request), std::move(onCompleted),
        [](Response response) {
            bool found = false;
            RuntimeData value;
            if (response.code == ServerErrorCode::None) {
                const auto* body = response.body.getIf<RuntimeData::Map>();
                if (body == nullptr || !body->contains("found") ||
                    body->at("found").getIf<bool>() == nullptr ||
                    !body->contains("value")) {
                    response = invalidResponse();
                } else {
                    found = *body->at("found").getIf<bool>();
                    value = body->at("value");
                }
            }
            return std::make_shared<ServerReadResult>(
                response.code, std::move(response.message), found,
                std::move(value));
        },
        std::move(error));
}

std::shared_ptr<AsyncOperation> Server::AcquireWriteTokenAsync(
    const std::string& accountId, TokenCallback onCompleted) {
    std::optional<Response> error;
    if (!ludork::global::server_impl::validIdentifier(accountId)) {
        error = invalidArgument();
    }
    Request request{"POST", accountPath(accountId) + "/write-tokens"};
    request.body = "{}";
    std::weak_ptr<void> session;
#if defined(LUDORK_SERVER_AVAILABLE)
    session = ludork::global::server_impl::transport().session();
#endif
    const auto expires =
        PendingRequest::Clock::now() + std::chrono::seconds(60);
    return schedule<ServerTokenResult>(
        std::move(request), std::move(onCompleted),
        [accountId, session, expires](Response response) {
            std::shared_ptr<ServerWriteToken> token;
            if (response.code == ServerErrorCode::None) {
                const auto* body = response.body.getIf<RuntimeData::Map>();
                bool valid = body != nullptr && body->contains("token") &&
                             body->contains("expiresAt");
                const std::string* raw =
                    valid ? body->at("token").getIf<std::string>() : nullptr;
                valid = valid && raw != nullptr && !raw->empty() &&
                        raw->size() <= 4096;
                if (valid) {
                    valid = std::all_of(raw->begin(), raw->end(),
                                        [](unsigned char byte) {
                                            return byte > 32 && byte < 127;
                                        });
                    const auto& expiry = body->at("expiresAt");
                    const auto* integer = expiry.getIf<std::int64_t>();
                    const auto* decimal = expiry.getIf<double>();
                    valid =
                        valid && ((integer != nullptr && *integer > 0) ||
                                  (decimal != nullptr &&
                                   std::isfinite(*decimal) && *decimal > 0));
                }
                if (!valid) {
                    response = invalidResponse();
                } else {
                    token =
                        std::shared_ptr<ServerWriteToken>(new ServerWriteToken(
                            accountId, *raw, expires, session));
                }
            }
            return std::make_shared<ServerTokenResult>(
                response.code, std::move(response.message), std::move(token));
        },
        std::move(error));
}

std::shared_ptr<AsyncOperation> Server::WriteFieldAsync(
    std::shared_ptr<ServerWriteToken> token, const std::string& category,
    const std::string& field, const RuntimeData& value,
    WriteCallback onCompleted) {
    if (!onCompleted) {
        throw std::invalid_argument(
            "Server operation requires a completion callback.");
    }
    std::optional<Response> error;
    Request request;
    request.method = "PUT";
    if (!ludork::global::server_impl::validIdentifier(category) ||
        !ludork::global::server_impl::validIdentifier(field)) {
        error = invalidArgument();
    } else {
        try {
            request.body =
                stringifyJSON(RuntimeData(RuntimeData::Map{{"value", value}}));
            if (request.body.size() > 1024 * 1024) {
                error = Response{ServerErrorCode::InvalidArgument,
                                 "The serialized write body exceeds 1 MiB."};
            }
        } catch (const std::exception&) {
            error = Response{
                ServerErrorCode::InvalidArgument,
                "The field value must contain finite JSON-compatible data."};
        }
    }
#if defined(LUDORK_SERVER_AVAILABLE)
    if (!error) {
        const auto session =
            token ? token->session_.lock() : std::shared_ptr<void>{};
        if (!token || !session ||
            session != ludork::global::server_impl::transport().session() ||
            token->consumed_ ||
            PendingRequest::Clock::now() >= token->expires_) {
            error = Response{
                ServerErrorCode::InvalidWriteToken,
                "The write token is invalid, expired or already consumed."};
        } else {
            request.path = fieldPath(token->account_, category, field);
            request.bearer = token->token_;
            token->consumed_ = true;
        }
    }
#else
    static_cast<void>(token);
#endif
    return schedule<ServerResult>(
        std::move(request), std::move(onCompleted),
        [](Response response) {
            if (response.code == ServerErrorCode::None) {
                const auto* body = response.body.getIf<RuntimeData::Map>();
                if (body == nullptr || !body->empty()) {
                    response = invalidResponse();
                }
            }
            return std::make_shared<ServerResult>(response.code,
                                                  std::move(response.message),
                                                  response.retryAfterSeconds);
        },
        std::move(error));
}

std::shared_ptr<AsyncOperation> Server::CheckConnectionAsync(
    ConnectionCallback onCompleted) {
    Request request{"GET", "/health"};
    request.health = true;
    return schedule<ServerConnectionResult>(
        std::move(request), std::move(onCompleted), [](Response response) {
            std::optional<double> roundTrip;
            if (response.code == ServerErrorCode::None) {
                roundTrip = response.roundTripMs;
            }
            return std::make_shared<ServerConnectionResult>(
                response.code, std::move(response.message), roundTrip);
        });
}

namespace ludork::global {
void initializeServer() {
#if defined(LUDORK_SERVER_AVAILABLE)
    server_impl::initializeTransport();
#endif
}
void shutdownServer() noexcept {
#if defined(LUDORK_SERVER_AVAILABLE)
    server_impl::shutdownTransport();
#endif
}
}  // namespace ludork::global
