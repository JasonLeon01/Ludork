#include <Server/ServerWriteToken.hpp>

#include <utility>

ServerWriteToken::ServerWriteToken(
    std::string account, std::string token,
    std::chrono::steady_clock::time_point expires, std::weak_ptr<void> session)
    : account_(std::move(account)),
      token_(std::move(token)),
      expires_(expires),
      session_(std::move(session)) {}
