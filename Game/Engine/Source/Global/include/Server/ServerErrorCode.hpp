#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>

BIND_ENUM()
enum class ServerErrorCode {
    None,
    Disabled,
    InvalidArgument,
    AuthenticationFailed,
    Timeout,
    ConnectionFailed,
    InvalidResponse,
    StorageFailure,
    ServerFailure,
    RateLimited
};
