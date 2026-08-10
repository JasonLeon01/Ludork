#include "NativeInputMethod.hpp"

#import <AppKit/AppKit.h>
#import <SFML/Window/macOS/SFOpenGLView.h>

@interface SFOpenGLView (LudorkInputMethod)
- (NSTextInputContext *)ludorkInputContext;
@end

@implementation SFOpenGLView (LudorkInputMethod)

- (NSTextInputContext *)ludorkInputContext {
  return [m_hiddenTextView inputContext];
}

@end

namespace ludork::global {
namespace {

NSTextInputContext *controlledInputContext = nil;
NSArray<NSString *> *previousAllowedInputSourceLocales = nil;
NSString *previousInputSource = nil;
bool inputMethodDisabled = false;

SFOpenGLView *findOpenGLView(NSView *view) {
  if (view == nil)
    return nil;
  if ([view isKindOfClass:[SFOpenGLView class]])
    return static_cast<SFOpenGLView *>(view);
  for (NSView *subview in [view subviews]) {
    if (SFOpenGLView *result = findOpenGLView(subview); result != nil)
      return result;
  }
  return nil;
}

NSTextInputContext *inputContextForHandle(sf::WindowHandle windowHandle) {
  id nativeHandle = static_cast<id>(windowHandle);
  NSView *rootView = nil;
  if ([nativeHandle isKindOfClass:[NSWindow class]])
    rootView = [static_cast<NSWindow *>(nativeHandle) contentView];
  else if ([nativeHandle isKindOfClass:[NSView class]])
    rootView = static_cast<NSView *>(nativeHandle);
  SFOpenGLView *openGLView = findOpenGLView(rootView);
  return openGLView == nil ? nil : [openGLView ludorkInputContext];
}

void clearSavedInputContext() noexcept {
  [controlledInputContext release];
  [previousAllowedInputSourceLocales release];
  [previousInputSource release];
  controlledInputContext = nil;
  previousAllowedInputSourceLocales = nil;
  previousInputSource = nil;
  inputMethodDisabled = false;
}

void restoreInputContext() noexcept {
  if (!inputMethodDisabled)
    return;
  [controlledInputContext discardMarkedText];
  [controlledInputContext
      setAllowedInputSourceLocales:previousAllowedInputSourceLocales];
  if (previousInputSource != nil &&
      [[controlledInputContext keyboardInputSources]
          containsObject:previousInputSource]) {
    [controlledInputContext setSelectedKeyboardInputSource:previousInputSource];
  }
  clearSavedInputContext();
}

} // namespace

bool setNativeInputMethodDisabled(sf::WindowHandle windowHandle,
                                  bool disabled) noexcept {
  @autoreleasepool {
    if (!disabled) {
      if (!inputMethodDisabled)
        return true;
      restoreInputContext();
      return true;
    }
    NSTextInputContext *inputContext = inputContextForHandle(windowHandle);
    if (inputContext == nil)
      return false;
    if (inputMethodDisabled && controlledInputContext == inputContext)
      return true;
    restoreInputContext();
    controlledInputContext = [inputContext retain];
    previousAllowedInputSourceLocales =
        [[inputContext allowedInputSourceLocales] copy];
    previousInputSource = [[inputContext selectedKeyboardInputSource] copy];
    [inputContext discardMarkedText];
    [inputContext setAllowedInputSourceLocales:@[
      NSAllRomanInputSourcesLocaleIdentifier
    ]];
    inputMethodDisabled = true;
    return true;
  }
}

void restoreNativeInputMethod() noexcept {
  @autoreleasepool {
    restoreInputContext();
  }
}

} // namespace ludork::global
