#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <RuntimeApi.hpp>

#include <string>

namespace ludork::runtime {

/// Open an independent native web overlay without pausing the game or caller.
/// HTTP and HTTPS URLs are supported. A later call navigates the existing
/// layer.
/// @param url Absolute webpage URL.
/// @return Whether the host accepted the request, not whether loading
/// succeeded.
BIND_FUNCTION(name = "OpenWebView", returns = "accepted")
LUDORK_RUNTIME_API bool OpenWebView(const std::string& url);

}  // namespace ludork::runtime
