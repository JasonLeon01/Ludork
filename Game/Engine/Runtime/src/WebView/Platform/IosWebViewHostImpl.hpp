#pragma once

#include <Runtime/WebViewHost.hpp>

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#include <atomic>
#include <mutex>

@class LudorkRuntimeIosWebViewControllerImpl;

namespace ludork::runtime::webview {

struct IosWebViewSessionImpl
    : std::enable_shared_from_this<IosWebViewSessionImpl> {
    std::atomic<std::uint64_t> id;
    sf::WindowHandle handle;
    std::atomic<bool> cancelled = false;
    UIWindow* gameWindow = nil;
    UIWindow* window = nil;
    LudorkRuntimeIosWebViewControllerImpl* controller = nil;
    NSObject* backgroundObserver = nil;

    IosWebViewSessionImpl(std::uint64_t sessionId,
                          sf::WindowHandle windowHandle);
    void open(const std::string& url);
    void close(bool notify);
};

class IosWebViewHostImpl final : public Host {
public:
    explicit IosWebViewHostImpl(sf::WindowHandle window);
    ~IosWebViewHostImpl() override;
    bool open(std::uint64_t id, const std::string& url) override;
    void close(std::uint64_t id) override;

private:
    sf::WindowHandle window_;
    std::mutex mutex_;
    std::shared_ptr<IosWebViewSessionImpl> session_;
};

}  // namespace ludork::runtime::webview

@interface LudorkRuntimeIosWebViewControllerImpl
    : UIViewController <WKNavigationDelegate, WKUIDelegate> {
@public
    std::weak_ptr<ludork::runtime::webview::IosWebViewSessionImpl> session;
}
@property(nonatomic, strong) WKWebView* webView;
@property(nonatomic, strong) WKNavigation* navigation;
@property(nonatomic, strong) UILabel* status;
- (void)closeWebView;
@end
