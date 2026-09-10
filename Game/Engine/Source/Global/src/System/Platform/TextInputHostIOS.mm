#include "TextInputHostIOSImpl.hpp"

#include <utility>

namespace {

NSString* nativeString(const std::string& value) {
    return [[[NSString alloc] initWithBytes:value.data()
                                     length:value.size()
                                   encoding:NSUTF8StringEncoding] autorelease];
}

}  // namespace

@implementation LudorkTextInputDialogImpl

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
    if (textField.markedTextRange != nil) {
        [textField unmarkText];
        return NO;
    }
    if (const auto value = session.lock()) {
        value->complete(true, true);
    }
    return NO;
}

@end

@implementation LudorkTextInputPresentationImpl

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}

@end

namespace ludork::global {

TextInputHostIOSImpl::Session::Session(
    ludork::engine::text_input::SessionId value, Sink callback,
    std::shared_ptr<std::atomic<bool>> live)
    : id(value), sink(std::move(callback)), windowLive(std::move(live)) {}

TextInputHostIOSImpl::Session::~Session() = default;

void TextInputHostIOSImpl::Session::complete(bool accepted, bool notify) {
    if (completed.exchange(true)) {
        return;
    }
    ludork::engine::text_input::Event event;
    event.kind = ludork::engine::text_input::Event::Kind::Complete;
    event.accepted = accepted;
    if (accepted && dialog != nil) {
        UITextField* field = dialog.textFields.firstObject;
        [field unmarkText];
        NSData* data = [field.text dataUsingEncoding:NSUTF8StringEncoding];
        if (data != nil && data.length > 0) {
            event.state.text.assign(static_cast<const char*>(data.bytes),
                                    data.length);
        }
    }
    if (backgroundObserver != nil) {
        [[NSNotificationCenter defaultCenter]
            removeObserver:backgroundObserver];
        [backgroundObserver release];
        backgroundObserver = nil;
    }
    dialog.textFields.firstObject.delegate = nil;
    [dialog dismissViewControllerAnimated:NO completion:nil];
    [dialog release];
    dialog = nil;
    const bool restoreWindow = presentationWindow.isKeyWindow;
    presentationWindow.hidden = YES;
    presentationWindow.rootViewController = nil;
    [presentationWindow release];
    presentationWindow = nil;
    if (restoreWindow && windowLive->load() && !gameWindow.hidden &&
        UIApplication.sharedApplication.applicationState ==
            UIApplicationStateActive) {
        [gameWindow makeKeyWindow];
    }
    [gameWindow release];
    gameWindow = nil;
    [delegate release];
    delegate = nil;
    if (notify) {
        sink(id, std::move(event));
    }
    sink = {};
}

TextInputHostIOSImpl::TextInputHostIOSImpl(sf::WindowHandle window)
    : window_(window), windowLive_(std::make_shared<std::atomic<bool>>(true)) {}

TextInputHostIOSImpl::~TextInputHostIOSImpl() {
    windowLive_->store(false);
    std::shared_ptr<Session> previous;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        previous = std::exchange(session_, nullptr);
    }
    if (previous != nullptr) {
        previous->cancelled.store(true);
        dispatch_async(dispatch_get_main_queue(),
                       ^{ previous->complete(false, false); });
    }
}

bool TextInputHostIOSImpl::isModal() const {
    return true;
}

bool TextInputHostIOSImpl::handlesKeyboard() const {
    return true;
}

bool TextInputHostIOSImpl::begin(
    ludork::engine::text_input::SessionId id,
    const ludork::engine::text_input::Request& request, Sink sink) {
    const std::shared_ptr<Session> next =
        std::make_shared<Session>(id, std::move(sink), windowLive_);
    std::shared_ptr<Session> previous;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        previous = std::exchange(session_, next);
    }
    const sf::WindowHandle handle = window_;
    const ludork::engine::text_input::Request snapshot = request;
    dispatch_async(dispatch_get_main_queue(), ^{
      @autoreleasepool {
          if (previous != nullptr) {
              previous->complete(false, false);
          }
          if (next->cancelled.load()) {
              next->complete(false, false);
              return;
          }
          UIWindow* window = static_cast<UIWindow*>(handle);
          if (window == nil ||
              UIApplication.sharedApplication.applicationState !=
                  UIApplicationStateActive) {
              next->complete(false, true);
              return;
          }
          next->gameWindow = [window retain];
          next->presentationWindow =
              window.windowScene != nil
                  ? [[UIWindow alloc] initWithWindowScene:window.windowScene]
                  : [[UIWindow alloc] initWithFrame:window.bounds];
          next->presentationWindow.frame = window.frame;
          next->presentationWindow.windowLevel = window.windowLevel + 1;
          next->presentationWindow.opaque = NO;
          next->presentationWindow.backgroundColor = UIColor.clearColor;
          LudorkTextInputPresentationImpl* presenter =
              [[LudorkTextInputPresentationImpl alloc] init];
          presenter.view.backgroundColor = UIColor.clearColor;
          next->presentationWindow.rootViewController = presenter;
          [presenter release];
          [next->presentationWindow makeKeyAndVisible];
          UIAlertController* dialog = [UIAlertController
              alertControllerWithTitle:nativeString(snapshot.title)
                               message:nativeString(snapshot.prompt)
                        preferredStyle:UIAlertControllerStyleAlert];
          next->dialog = [dialog retain];
          LudorkTextInputDialogImpl* delegate =
              [[LudorkTextInputDialogImpl alloc] init];
          delegate->session = next;
          next->delegate = delegate;
          [dialog addTextFieldWithConfigurationHandler:^(UITextField* field) {
            field.text = nativeString(snapshot.state.text);
            field.placeholder = nativeString(snapshot.placeholder);
            field.autocorrectionType = UITextAutocorrectionTypeNo;
            field.autocapitalizationType = UITextAutocapitalizationTypeNone;
            field.returnKeyType = UIReturnKeyDone;
            field.clearButtonMode = UITextFieldViewModeWhileEditing;
            field.delegate = delegate;
          }];
          [dialog
              addAction:[UIAlertAction
                            actionWithTitle:nativeString(snapshot.cancelText)
                                      style:UIAlertActionStyleCancel
                                    handler:^(UIAlertAction*) {
                                      next->complete(false, true);
                                    }]];
          [dialog
              addAction:[UIAlertAction
                            actionWithTitle:nativeString(snapshot.confirmText)
                                      style:UIAlertActionStyleDefault
                                    handler:^(UIAlertAction*) {
                                      next->complete(true, true);
                                    }]];
          next->backgroundObserver = [[[NSNotificationCenter defaultCenter]
              addObserverForName:UIApplicationDidEnterBackgroundNotification
                          object:nil
                           queue:NSOperationQueue.mainQueue
                      usingBlock:^(NSNotification*) {
                        next->complete(false, true);
                      }] retain];
          [presenter
              presentViewController:dialog
                           animated:YES
                         completion:^{
                           if (next->completed.load()) {
                               return;
                           }
                           [dialog.textFields.firstObject becomeFirstResponder];
                         }];
      }
    });
    return true;
}

void TextInputHostIOSImpl::update(ludork::engine::text_input::SessionId,
                                  const ludork::engine::text_input::State&,
                                  const sf::FloatRect&) {}

void TextInputHostIOSImpl::end(ludork::engine::text_input::SessionId id) {
    std::shared_ptr<Session> previous;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (session_ == nullptr || session_->id != id) {
            return;
        }
        previous = std::exchange(session_, nullptr);
    }
    previous->cancelled.store(true);
    dispatch_async(dispatch_get_main_queue(),
                   ^{ previous->complete(false, false); });
}

std::shared_ptr<ludork::engine::text_input::TextInputHost>
createIosTextInputHost(sf::WindowHandle window) {
    return std::make_shared<TextInputHostIOSImpl>(window);
}

}  // namespace ludork::global
