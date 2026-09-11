#pragma once

#include <RuntimeApi.hpp>
#include <SFML/Window/WindowHandle.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace ludork::runtime::webview {

class LUDORK_RUNTIME_API Host {
public:
    virtual ~Host() = default;
    // Return immediately; false must leave an existing presentation unchanged.
    // Every accepted request replaces the current ID and navigates the same
    // layer.
    virtual bool open(std::uint64_t id, const std::string& url) = 0;
    virtual void close(std::uint64_t id) = 0;
};

using HostFactory = std::function<std::shared_ptr<Host>(sf::WindowHandle)>;

LUDORK_RUNTIME_API void setHostFactory(HostFactory factory);
LUDORK_RUNTIME_API void attachWindow(sf::WindowHandle window);
LUDORK_RUNTIME_API void detachWindow();
LUDORK_RUNTIME_API void notifyClosed(std::uint64_t id);

}  // namespace ludork::runtime::webview
