#pragma once

#include <RuntimeApi.hpp>

#include <cstdint>

namespace ludork::runtime::webview {

[[nodiscard]] LUDORK_RUNTIME_API bool isInputBlocked();
[[nodiscard]] LUDORK_RUNTIME_API std::uint64_t inputRevision();

}  // namespace ludork::runtime::webview
