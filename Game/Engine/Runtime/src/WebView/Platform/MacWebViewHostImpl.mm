#include "MacWebViewHostImpl.hpp"
#include "WebViewPlatform.hpp"

#include <algorithm>
#include <utility>

namespace {

bool allowedRequest(NSURLRequest* request) {
    NSString* scheme = request.URL.scheme.lowercaseString;
    return
        [scheme isEqualToString:@"https"] || [scheme isEqualToString:@"http"];
}

}  // namespace

@implementation LudorkRuntimeWebViewWindowImpl
- (BOOL)canBecomeKeyWindow {
    return YES;
}
@end

@implementation LudorkRuntimeWebViewDelegateImpl

- (void)closeWebView:(id)sender {
    static_cast<void>(sender);
    if (const auto state = session.lock()) {
        state->close(true);
    }
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)action
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
    static_cast<void>(webView);
    decisionHandler(allowedRequest(action.request) &&
                            !action.shouldPerformDownload
                        ? WKNavigationActionPolicyAllow
                        : WKNavigationActionPolicyCancel);
}

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationResponse:(WKNavigationResponse*)response
                      decisionHandler:(void (^)(WKNavigationResponsePolicy))
                                          decisionHandler {
    static_cast<void>(webView);
    decisionHandler(response.canShowMIMEType
                        ? WKNavigationResponsePolicyAllow
                        : WKNavigationResponsePolicyCancel);
}

- (WKWebView*)webView:(WKWebView*)webView
    createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration
               forNavigationAction:(WKNavigationAction*)action
                    windowFeatures:(WKWindowFeatures*)features {
    static_cast<void>(configuration);
    static_cast<void>(features);
    if (allowedRequest(action.request) && !action.shouldPerformDownload) {
        [webView loadRequest:action.request];
    }
    return nil;
}

- (void)webView:(WKWebView*)webView
    didStartProvisionalNavigation:(WKNavigation*)navigation {
    static_cast<void>(webView);
    if (const auto state = session.lock()) {
        state->navigation = navigation;
        state->status.stringValue = @"Loading…";
    }
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
    static_cast<void>(webView);
    if (const auto state = session.lock();
        state != nullptr && state->navigation == navigation) {
        state->status.stringValue = @"";
    }
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
    static_cast<void>(webView);
    if (error.code != NSURLErrorCancelled) {
        if (const auto state = session.lock();
            state != nullptr && state->navigation == navigation) {
            state->showError(error.localizedDescription);
        }
    }
}

- (void)webView:(WKWebView*)webView
    didFailNavigation:(WKNavigation*)navigation
            withError:(NSError*)error {
    [self webView:webView
        didFailProvisionalNavigation:navigation
                           withError:error];
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView {
    static_cast<void>(webView);
    if (const auto state = session.lock()) {
        state->showError(@"Web content stopped. Close and reopen this page.");
    }
}

- (void)webViewDidClose:(WKWebView*)webView {
    static_cast<void>(webView);
    [self closeWebView:nil];
}

- (void)webView:(WKWebView*)webView
    requestMediaCapturePermissionForOrigin:(WKSecurityOrigin*)origin
                          initiatedByFrame:(WKFrameInfo*)frame
                                      type:(WKMediaCaptureType)type
                           decisionHandler:
                               (void (^)(WKPermissionDecision))decisionHandler {
    static_cast<void>(webView);
    static_cast<void>(origin);
    static_cast<void>(frame);
    static_cast<void>(type);
    decisionHandler(WKPermissionDecisionDeny);
}

@end

namespace ludork::runtime::webview {

MacWebViewSessionImpl::MacWebViewSessionImpl(std::uint64_t sessionId,
                                             sf::WindowHandle windowHandle)
    : id(sessionId), handle(windowHandle) {}

void MacWebViewSessionImpl::resize() {
    if (parent == nil || window == nil) {
        return;
    }
    const NSRect frame = [parent convertRectToScreen:parent.contentView.bounds];
    [window setFrame:frame display:YES];
    NSView* content = window.contentView;
    webView.frame =
        NSMakeRect(0, 0, content.bounds.size.width,
                   std::max<CGFloat>(0, content.bounds.size.height - 44));
    status.frame =
        NSMakeRect(16, content.bounds.size.height - 34,
                   std::max<CGFloat>(0, content.bounds.size.width - 112), 24);
}

void MacWebViewSessionImpl::showError(NSString* message) {
    status.stringValue = message ?: @"Unable to load this page.";
    status.toolTip = status.stringValue;
}

void MacWebViewSessionImpl::open(const std::string& url) {
    if (cancelled) {
        return;
    }
    if (window == nil) {
        NSObject* native = (__bridge NSObject*)handle;
        parent = [native isKindOfClass:NSWindow.class]
                     ? static_cast<NSWindow*>(native)
                     : [static_cast<NSView*>(native) window];
        if (parent == nil) {
            close(true);
            return;
        }
        window = [[LudorkRuntimeWebViewWindowImpl alloc]
            initWithContentRect:NSZeroRect
                      styleMask:NSWindowStyleMaskBorderless
                        backing:NSBackingStoreBuffered
                          defer:NO];
        window.releasedWhenClosed = NO;
        window.backgroundColor = NSColor.windowBackgroundColor;
        window.collectionBehavior =
            NSWindowCollectionBehaviorFullScreenAuxiliary;
        delegate = [[LudorkRuntimeWebViewDelegateImpl alloc] init];
        delegate->session = weak_from_this();
        WKWebViewConfiguration* configuration =
            [[WKWebViewConfiguration alloc] init];
        webView = [[WKWebView alloc] initWithFrame:NSZeroRect
                                     configuration:configuration];
        webView.navigationDelegate = delegate;
        webView.UIDelegate = delegate;
        [window.contentView addSubview:webView];
        status = [NSTextField labelWithString:@"Loading…"];
        status.lineBreakMode = NSLineBreakByTruncatingTail;
        [window.contentView addSubview:status];
        NSButton* closeButton =
            [NSButton buttonWithTitle:@"Close"
                               target:delegate
                               action:@selector(closeWebView:)];
        closeButton.translatesAutoresizingMaskIntoConstraints = NO;
        [window.contentView addSubview:closeButton];
        [NSLayoutConstraint activateConstraints:@[
            [closeButton.trailingAnchor
                constraintEqualToAnchor:window.contentView.trailingAnchor
                               constant:-12],
            [closeButton.topAnchor
                constraintEqualToAnchor:window.contentView.topAnchor
                               constant:8]
        ]];
        const std::weak_ptr<MacWebViewSessionImpl> weak = weak_from_this();
        resizeObserver = [NSNotificationCenter.defaultCenter
            addObserverForName:NSWindowDidResizeNotification
                        object:parent
                         queue:nil
                    usingBlock:^(NSNotification*) {
                      if (const auto state = weak.lock()) {
                          state->resize();
                      }
                    }];
        closeObserver = [NSNotificationCenter.defaultCenter
            addObserverForName:NSWindowWillCloseNotification
                        object:parent
                         queue:nil
                    usingBlock:^(NSNotification*) {
                      if (const auto state = weak.lock()) {
                          state->close(true);
                      }
                    }];
        resize();
        [parent addChildWindow:window ordered:NSWindowAbove];
        [window makeKeyAndOrderFront:nil];
        [window makeFirstResponder:webView];
    }
    NSString* address = [[NSString alloc] initWithBytes:url.data()
                                                 length:url.size()
                                               encoding:NSUTF8StringEncoding];
    NSURL* target = [NSURL URLWithString:address];
    if (target == nil || target.host.length == 0) {
        showError(@"Invalid web address.");
        return;
    }
    [webView loadRequest:[NSURLRequest requestWithURL:target]];
}

void MacWebViewSessionImpl::close(bool notify) {
    const std::uint64_t previousId = id.exchange(0);
    cancelled = true;
    if (resizeObserver != nil) {
        [NSNotificationCenter.defaultCenter removeObserver:resizeObserver];
        resizeObserver = nil;
    }
    if (closeObserver != nil) {
        [NSNotificationCenter.defaultCenter removeObserver:closeObserver];
        closeObserver = nil;
    }
    [webView stopLoading];
    webView.navigationDelegate = nil;
    webView.UIDelegate = nil;
    [parent removeChildWindow:window];
    [window orderOut:nil];
    [window close];
    if (parent.visible) {
        [parent makeKeyAndOrderFront:nil];
    }
    webView = nil;
    navigation = nil;
    status = nil;
    delegate = nil;
    window = nil;
    parent = nil;
    if (notify) {
        notifyClosed(previousId);
    }
}

MacWebViewHostImpl::MacWebViewHostImpl(sf::WindowHandle window)
    : window_(window) {}

MacWebViewHostImpl::~MacWebViewHostImpl() {
    close(0);
}

bool MacWebViewHostImpl::open(std::uint64_t id, const std::string& url) {
    const std::lock_guard lock(mutex_);
    std::uint64_t previous = session_ == nullptr ? 0 : session_->id.load();
    const bool reuse =
        previous != 0 && session_->id.compare_exchange_strong(previous, id);
    if (!reuse) {
        session_ = std::make_shared<MacWebViewSessionImpl>(id, window_);
    }
    const auto state = session_;
    const std::string address = url;
    dispatch_async(dispatch_get_main_queue(), ^{ state->open(address); });
    return true;
}

void MacWebViewHostImpl::close(std::uint64_t id) {
    std::shared_ptr<MacWebViewSessionImpl> state;
    {
        const std::lock_guard lock(mutex_);
        if (session_ == nullptr || (id != 0 && session_->id.load() != id)) {
            return;
        }
        state = std::exchange(session_, nullptr);
        state->cancelled = true;
    }
    if (NSThread.isMainThread) {
        state->close(false);
    } else {
        dispatch_async(dispatch_get_main_queue(), ^{ state->close(false); });
    }
}

std::shared_ptr<Host> createPlatformHost(sf::WindowHandle window) {
    return std::make_shared<MacWebViewHostImpl>(window);
}

}  // namespace ludork::runtime::webview
