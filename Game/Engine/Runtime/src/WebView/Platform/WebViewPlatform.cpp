#include "WebViewPlatform.hpp"

namespace ludork::runtime::webview {

std::shared_ptr<Host> createPlatformHost(sf::WindowHandle) {
    return nullptr;
}

}  // namespace ludork::runtime::webview
