#pragma once

#include <Runtime/WebViewHost.hpp>

#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>

#include <atomic>
#include <mutex>

@class LudorkRuntimeWebViewDelegateImpl;

namespace ludork::runtime::webview {

struct MacWebViewSessionImpl
    : std::enable_shared_from_this<MacWebViewSessionImpl> {
    std::atomic<std::uint64_t> id;
    sf::WindowHandle handle;
    std::atomic<bool> cancelled = false;
    NSWindow* parent = nil;
    NSWindow* window = nil;
    WKWebView* webView = nil;
    WKNavigation* navigation = nil;
    NSTextField* status = nil;
    LudorkRuntimeWebViewDelegateImpl* delegate = nil;
    NSObject* resizeObserver = nil;
    NSObject* closeObserver = nil;

    MacWebViewSessionImpl(std::uint64_t sessionId,
                          sf::WindowHandle windowHandle);
    void open(const std::string& url);
    void close(bool notify);
    void resize();
    void showError(NSString* message);
};

class MacWebViewHostImpl final : public Host {
public:
    explicit MacWebViewHostImpl(sf::WindowHandle window);
    ~MacWebViewHostImpl() override;
    bool open(std::uint64_t id, const std::string& url) override;
    void close(std::uint64_t id) override;

private:
    sf::WindowHandle window_;
    std::mutex mutex_;
    std::shared_ptr<MacWebViewSessionImpl> session_;
};

}  // namespace ludork::runtime::webview

@interface LudorkRuntimeWebViewWindowImpl : NSWindow
@end

@interface LudorkRuntimeWebViewDelegateImpl
    : NSObject <WKNavigationDelegate, WKUIDelegate> {
@public
    std::weak_ptr<ludork::runtime::webview::MacWebViewSessionImpl> session;
}
- (void)closeWebView:(id)sender;
@end
