#include "IosWebViewHostImpl.hpp"
#include "WebViewPlatform.hpp"

#include <utility>

namespace {

bool allowedRequest(NSURLRequest* request) {
    NSString* scheme = request.URL.scheme.lowercaseString;
    return
        [scheme isEqualToString:@"https"] || [scheme isEqualToString:@"http"];
}

}  // namespace

@implementation LudorkRuntimeIosWebViewControllerImpl

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.systemBackgroundColor;
    self.webView = [[WKWebView alloc] initWithFrame:CGRectZero];
    self.webView.navigationDelegate = self;
    self.webView.UIDelegate = self;
    self.webView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.webView];
    self.status = [[UILabel alloc] initWithFrame:CGRectZero];
    self.status.text = @"Loading…";
    self.status.font = [UIFont systemFontOfSize:13];
    self.status.lineBreakMode = NSLineBreakByTruncatingTail;
    self.status.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.status];
    UIButton* close = [UIButton buttonWithType:UIButtonTypeSystem];
    [close setTitle:@"Close" forState:UIControlStateNormal];
    [close addTarget:self
                  action:@selector(closeWebView)
        forControlEvents:UIControlEventTouchUpInside];
    close.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:close];
    UILayoutGuide* safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [close.topAnchor constraintEqualToAnchor:safe.topAnchor],
        [close.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor
                                             constant:-12],
        [close.heightAnchor constraintEqualToConstant:44],
        [close.widthAnchor constraintGreaterThanOrEqualToConstant:64],
        [self.status.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor
                                                  constant:12],
        [self.status.trailingAnchor constraintEqualToAnchor:close.leadingAnchor
                                                   constant:-12],
        [self.status.centerYAnchor constraintEqualToAnchor:close.centerYAnchor],
        [self.webView.topAnchor constraintEqualToAnchor:close.bottomAnchor],
        [self.webView.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],
        [self.webView.trailingAnchor
            constraintEqualToAnchor:safe.trailingAnchor],
        [self.webView.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor]
    ]];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (void)closeWebView {
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
    self.navigation = navigation;
    self.status.text = @"Loading…";
}

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
    static_cast<void>(webView);
    if (self.navigation == navigation) {
        self.status.text = @"";
    }
}

- (void)webView:(WKWebView*)webView
    didFailProvisionalNavigation:(WKNavigation*)navigation
                       withError:(NSError*)error {
    static_cast<void>(webView);
    if (error.code != NSURLErrorCancelled && self.navigation == navigation) {
        self.status.text = error.localizedDescription;
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
    self.status.text = @"Web content stopped. Close and reopen this page.";
}

- (void)webViewDidClose:(WKWebView*)webView {
    static_cast<void>(webView);
    [self closeWebView];
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

IosWebViewSessionImpl::IosWebViewSessionImpl(std::uint64_t sessionId,
                                             sf::WindowHandle windowHandle)
    : id(sessionId), handle(windowHandle) {}

void IosWebViewSessionImpl::open(const std::string& url) {
    if (cancelled) {
        return;
    }
    if (window == nil) {
        gameWindow = (__bridge UIWindow*)handle;
        if (gameWindow == nil ||
            UIApplication.sharedApplication.applicationState !=
                UIApplicationStateActive) {
            close(true);
            return;
        }
        window =
            gameWindow.windowScene != nil
                ? [[UIWindow alloc] initWithWindowScene:gameWindow.windowScene]
                : [[UIWindow alloc] initWithFrame:gameWindow.bounds];
        window.frame = gameWindow.frame;
        window.windowLevel = gameWindow.windowLevel + 1;
        controller = [[LudorkRuntimeIosWebViewControllerImpl alloc] init];
        controller->session = weak_from_this();
        window.rootViewController = controller;
        [window makeKeyAndVisible];
        const std::weak_ptr<IosWebViewSessionImpl> weak = weak_from_this();
        backgroundObserver = [NSNotificationCenter.defaultCenter
            addObserverForName:UIApplicationDidEnterBackgroundNotification
                        object:nil
                         queue:nil
                    usingBlock:^(NSNotification*) {
                      if (const auto state = weak.lock()) {
                          state->close(true);
                      }
                    }];
    }
    NSString* address = [[NSString alloc] initWithBytes:url.data()
                                                 length:url.size()
                                               encoding:NSUTF8StringEncoding];
    NSURL* target = [NSURL URLWithString:address];
    if (target == nil || target.host.length == 0) {
        controller.status.text = @"Invalid web address.";
        return;
    }
    [controller.webView loadRequest:[NSURLRequest requestWithURL:target]];
}

void IosWebViewSessionImpl::close(bool notify) {
    const std::uint64_t previousId = id.exchange(0);
    cancelled = true;
    if (backgroundObserver != nil) {
        [NSNotificationCenter.defaultCenter removeObserver:backgroundObserver];
        backgroundObserver = nil;
    }
    [controller.webView stopLoading];
    controller.webView.navigationDelegate = nil;
    controller.webView.UIDelegate = nil;
    window.hidden = YES;
    window.rootViewController = nil;
    if (gameWindow != nil) {
        [gameWindow makeKeyAndVisible];
    }
    controller = nil;
    window = nil;
    gameWindow = nil;
    if (notify) {
        notifyClosed(previousId);
    }
}

IosWebViewHostImpl::IosWebViewHostImpl(sf::WindowHandle window)
    : window_(window) {}

IosWebViewHostImpl::~IosWebViewHostImpl() {
    close(0);
}

bool IosWebViewHostImpl::open(std::uint64_t id, const std::string& url) {
    const std::lock_guard lock(mutex_);
    std::uint64_t previous = session_ == nullptr ? 0 : session_->id.load();
    const bool reuse =
        previous != 0 && session_->id.compare_exchange_strong(previous, id);
    if (!reuse) {
        session_ = std::make_shared<IosWebViewSessionImpl>(id, window_);
    }
    const auto state = session_;
    const std::string address = url;
    dispatch_async(dispatch_get_main_queue(), ^{ state->open(address); });
    return true;
}

void IosWebViewHostImpl::close(std::uint64_t id) {
    std::shared_ptr<IosWebViewSessionImpl> state;
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
    return std::make_shared<IosWebViewHostImpl>(window);
}

}  // namespace ludork::runtime::webview
