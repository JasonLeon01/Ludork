#pragma once

#include <Runtime/WebViewHost.hpp>

namespace ludork::runtime::webview {

std::shared_ptr<Host> createPlatformHost(sf::WindowHandle window);

}  // namespace ludork::runtime::webview
