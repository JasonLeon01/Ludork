#include "DesktopTextInputHost.hpp"
#include "MacTextInputHostImpl.hpp"
#include "NativeInputMethod.hpp"

#include <SFML/System/String.hpp>
#include <algorithm>
#include <utility>

namespace {

std::string utf8(NSString* value) {
    if (value == nil) {
        return {};
    }
    const NSData* data = [value dataUsingEncoding:NSUTF8StringEncoding];
    return {static_cast<const char*>([data bytes]), [data length]};
}

NSString* nativeString(const std::string& value) {
    return [[[NSString alloc] initWithBytes:value.data()
                                     length:value.size()
                                   encoding:NSUTF8StringEncoding] autorelease];
}

std::size_t utf16Offset(const std::string& value, std::size_t bytes) {
    return sf::String::fromUtf8(value.begin(),
                                value.begin() + std::min(bytes, value.size()))
        .toUtf16()
        .size();
}

void onMain(void (^action)()) {
    if ([NSThread isMainThread]) {
        action();
    } else {
        dispatch_sync(dispatch_get_main_queue(), action);
    }
}

}  // namespace

@implementation LudorkTextInputClientImpl

- (BOOL)acceptsFirstResponder {
    return YES;
}
- (NSView*)hitTest:(NSPoint)point {
    return nil;
}
- (NSTextInputContext*)inputContext {
    return textContext;
}
- (void)keyDown:(NSEvent*)event {
    [textContext handleEvent:event];
}
- (BOOL)hasMarkedText {
    return !textState.preedit.empty();
}

- (NSRange)markedRange {
    if (textState.preedit.empty()) {
        return NSMakeRange(NSNotFound, 0);
    }
    return NSMakeRange(utf16Offset(textState.text,
                                   std::min(textState.anchor, textState.caret)),
                       [nativeString(textState.preedit) length]);
}

- (NSRange)selectedRange {
    if (!textState.preedit.empty()) {
        return NSMakeRange(
            [self markedRange].location +
                utf16Offset(textState.preedit, textState.preeditCaret),
            0);
    }
    const std::size_t first = std::min(textState.anchor, textState.caret);
    const std::size_t last = std::max(textState.anchor, textState.caret);
    const std::size_t start = utf16Offset(textState.text, first);
    return NSMakeRange(start, utf16Offset(textState.text, last) - start);
}

- (std::optional<std::pair<std::size_t, std::size_t>>)selectionForReplacement:
    (NSRange)range {
    if (range.location == NSNotFound) {
        return std::nullopt;
    }
    const std::size_t first = std::min(textState.anchor, textState.caret);
    const std::size_t last = std::max(textState.anchor, textState.caret);
    const std::size_t start = utf16Offset(textState.text, first);
    const std::size_t end = utf16Offset(textState.text, last);
    const std::size_t markedLength = [nativeString(textState.preedit) length];
    const std::u16string base =
        sf::String::fromUtf8(textState.text.begin(), textState.text.end())
            .toUtf16();
    auto bytePosition = [&](NSUInteger position) {
        std::size_t offset = position;
        if (markedLength != 0 && position > start) {
            offset = position < start + markedLength
                         ? start
                         : position - markedLength + end - start;
        }
        offset = std::min(offset, base.size());
        return sf::String::fromUtf16(base.begin(), base.begin() + offset)
            .toUtf8()
            .size();
    };
    if (markedLength != 0 && range.location >= start &&
        NSMaxRange(range) <= start + markedLength) {
        return std::nullopt;
    }
    return std::pair{bytePosition(range.location),
                     bytePosition(NSMaxRange(range))};
}

- (void)setMarkedText:(id)value
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
    NSString* string = [value isKindOfClass:[NSAttributedString class]]
                           ? [value string]
                           : value;
    const auto selection = [self selectionForReplacement:replacementRange];
    textState.preedit = utf8(string);
    const NSUInteger position =
        std::min(selectedRange.location, [string length]);
    textState.preeditCaret = utf8([string substringToIndex:position]).size();
    if (eventSink) {
        ludork::engine::text_input::Event event;
        event.kind = ludork::engine::text_input::Event::Kind::Preedit;
        event.replacementSelection = selection;
        event.state.preedit = textState.preedit;
        event.state.preeditCaret = textState.preeditCaret;
        eventSink(sessionId, std::move(event));
    }
}

- (void)unmarkText {
    textState.preedit.clear();
    textState.preeditCaret = 0;
    if (eventSink) {
        ludork::engine::text_input::Event event;
        event.kind = ludork::engine::text_input::Event::Kind::Preedit;
        eventSink(sessionId, std::move(event));
    }
}

- (void)insertText:(id)value replacementRange:(NSRange)replacementRange {
    NSString* string = [value isKindOfClass:[NSAttributedString class]]
                           ? [value string]
                           : value;
    const auto selection = [self selectionForReplacement:replacementRange];
    textState.preedit.clear();
    textState.preeditCaret = 0;
    if (eventSink) {
        ludork::engine::text_input::Event event;
        event.kind = ludork::engine::text_input::Event::Kind::Insert;
        event.text = utf8(string);
        event.replacementSelection = selection;
        eventSink(sessionId, std::move(event));
    }
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText {
    return @[];
}
- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:
                                                   (NSRangePointer)actualRange {
    NSMutableString* content =
        [[nativeString(textState.text) mutableCopy] autorelease];
    if (!textState.preedit.empty()) {
        const std::size_t first = std::min(textState.anchor, textState.caret);
        const std::size_t last = std::max(textState.anchor, textState.caret);
        const std::size_t begin = utf16Offset(textState.text, first);
        [content replaceCharactersInRange:NSMakeRange(
                                              begin, utf16Offset(textState.text,
                                                                 last) -
                                                         begin)
                               withString:nativeString(textState.preedit)];
    }
    const NSRange effective =
        NSIntersectionRange(range, NSMakeRange(0, [content length]));
    if (actualRange != nullptr) {
        *actualRange = effective;
    }
    return [[[NSAttributedString alloc]
        initWithString:[content substringWithRange:effective]] autorelease];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    return [self selectedRange].location;
}
- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actualRange {
    if (actualRange != nullptr) {
        *actualRange = range;
    }
    if (surfaceView == nil || [surfaceView window] == nil) {
        return NSZeroRect;
    }
    const NSRect bounds = [surfaceView bounds];
    const CGFloat scaleX = bounds.size.width / std::max(1u, surfaceSize.x);
    const CGFloat scaleY = bounds.size.height / std::max(1u, surfaceSize.y);
    NSRect rect = NSMakeRect(caretRectangle.position.x * scaleX,
                             caretRectangle.position.y * scaleY,
                             std::max(1.0f, caretRectangle.size.x) * scaleX,
                             std::max(1.0f, caretRectangle.size.y) * scaleY);
    if (![surfaceView isFlipped]) {
        rect.origin.y = bounds.size.height - NSMaxY(rect);
    }
    return [[surfaceView window]
        convertRectToScreen:[surfaceView convertRect:rect toView:nil]];
}

- (void)sendCommand:(ludork::engine::text_input::Command)command
             extend:(BOOL)extend {
    if (eventSink) {
        ludork::engine::text_input::Event event;
        event.kind = ludork::engine::text_input::Event::Kind::Command;
        event.command = command;
        event.extendSelection = extend;
        eventSink(sessionId, std::move(event));
    }
}

- (NSEvent*)handleNativeEvent:(NSEvent*)event {
    if ([event window] != [self window]) {
        return event;
    }
    const bool enter = [event keyCode] == 36 || [event keyCode] == 76;
    const bool space = [event keyCode] == 49;
    if ((enter && suppressEnter) || (space && suppressSpace)) {
        if ([event type] == NSEventTypeKeyUp) {
            if (enter) {
                suppressEnter = false;
            }
            if (space) {
                suppressSpace = false;
            }
        }
        return nil;
    }
    if ([event type] == NSEventTypeKeyDown) {
        const bool shortcut =
            ([event modifierFlags] & NSEventModifierFlagCommand) != 0;
        NSString* key = [[event charactersIgnoringModifiers] lowercaseString];
        if (shortcut && [key isEqualToString:@"a"]) {
            [self sendCommand:ludork::engine::text_input::Command::SelectAll
                       extend:NO];
        } else if (shortcut && [key isEqualToString:@"c"]) {
            [self sendCommand:ludork::engine::text_input::Command::Copy
                       extend:NO];
        } else if (shortcut && [key isEqualToString:@"x"]) {
            [self sendCommand:ludork::engine::text_input::Command::Cut
                       extend:NO];
        } else if (shortcut && [key isEqualToString:@"v"]) {
            [self sendCommand:ludork::engine::text_input::Command::Paste
                       extend:NO];
        } else {
            [self keyDown:event];
        }
    }
    return nil;
}

- (void)doCommandBySelector:(SEL)selector {
    if (selector == @selector(moveLeft:) ||
        selector == @selector(moveBackward:)) {
        [self sendCommand:ludork::engine::text_input::Command::Left extend:NO];
    } else if (selector == @selector(moveRight:) ||
               selector == @selector(moveForward:)) {
        [self sendCommand:ludork::engine::text_input::Command::Right extend:NO];
    } else if (selector == @selector(moveLeftAndModifySelection:) ||
               selector == @selector(moveBackwardAndModifySelection:)) {
        [self sendCommand:ludork::engine::text_input::Command::Left extend:YES];
    } else if (selector == @selector(moveRightAndModifySelection:) ||
               selector == @selector(moveForwardAndModifySelection:)) {
        [self sendCommand:ludork::engine::text_input::Command::Right
                   extend:YES];
    } else if (selector == @selector(moveToBeginningOfLine:) ||
               selector == @selector(moveToBeginningOfDocument:) ||
               selector == @selector(scrollToBeginningOfDocument:)) {
        [self sendCommand:ludork::engine::text_input::Command::Home extend:NO];
    } else if (selector == @selector(moveToEndOfLine:) ||
               selector == @selector(moveToEndOfDocument:) ||
               selector == @selector(scrollToEndOfDocument:)) {
        [self sendCommand:ludork::engine::text_input::Command::End extend:NO];
    } else if (selector ==
                   @selector(moveToBeginningOfLineAndModifySelection:) ||
               selector ==
                   @selector(moveToBeginningOfDocumentAndModifySelection:)) {
        [self sendCommand:ludork::engine::text_input::Command::Home extend:YES];
    } else if (selector == @selector(moveToEndOfLineAndModifySelection:) ||
               selector == @selector(moveToEndOfDocumentAndModifySelection:)) {
        [self sendCommand:ludork::engine::text_input::Command::End extend:YES];
    } else if (selector == @selector(deleteBackward:)) {
        [self sendCommand:ludork::engine::text_input::Command::Backspace
                   extend:NO];
    } else if (selector == @selector(deleteForward:)) {
        [self sendCommand:ludork::engine::text_input::Command::Delete
                   extend:NO];
    } else if (selector == @selector(insertNewline:)) {
        [self sendCommand:ludork::engine::text_input::Command::Finish
                   extend:NO];
    } else if (selector == @selector(cancelOperation:)) {
        [self sendCommand:ludork::engine::text_input::Command::Cancel
                   extend:NO];
    }
}

@end

namespace ludork::global {

MacTextInputHostImpl::MacTextInputHostImpl(sf::WindowBase& window)
    : window_(window) {}
MacTextInputHostImpl::~MacTextInputHostImpl() {
    end(id_);
}
bool MacTextInputHostImpl::isModal() const {
    return false;
}
bool MacTextInputHostImpl::handlesKeyboard() const {
    return true;
}

bool MacTextInputHostImpl::begin(
    ludork::engine::text_input::SessionId id,
    const ludork::engine::text_input::Request& request, Sink sink) {
    id_ = id;
    const sf::WindowHandle handle = window_.getNativeHandle();
    const sf::Vector2u size = window_.getSize();
    const bool enterHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter);
    const bool spaceHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    onMain(^{
      @autoreleasepool {
          setNativeInputMethodDisabled(handle, false);
          NSObject* native = static_cast<NSObject*>(handle);
          NSView* surface = [native isKindOfClass:[NSWindow class]]
                                ? [static_cast<NSWindow*>(native) contentView]
                                : static_cast<NSView*>(native);
          nativeWindow_ = [[surface window] retain];
          if (nativeWindow_ == nil) {
              return;
          }
          previousResponder_ = [[nativeWindow_ firstResponder] retain];
          client_ =
              [[LudorkTextInputClientImpl alloc] initWithFrame:NSZeroRect];
          client_->sessionId = id_;
          client_->textState = request.state;
          client_->eventSink = sink;
          client_->caretRectangle = request.caretRect;
          client_->surfaceView = surface;
          client_->surfaceSize = size;
          client_->textContext =
              [[NSTextInputContext alloc] initWithClient:client_];
          [surface addSubview:client_];
          [nativeWindow_ makeFirstResponder:client_];
          [client_->textContext activate];
          LudorkTextInputClientImpl* client = client_;
          client_->suppressEnter = enterHeld;
          client_->suppressSpace = spaceHeld;
          eventMonitor_ = [[NSEvent
              addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown |
                                                   NSEventMaskKeyUp |
                                                   NSEventMaskFlagsChanged
                                           handler:^NSEvent*(NSEvent* event) {
                                             return [client
                                                 handleNativeEvent:event];
                                           }] retain];
      }
    });
    return client_ != nil;
}

void MacTextInputHostImpl::update(
    ludork::engine::text_input::SessionId id,
    const ludork::engine::text_input::State& state,
    const sf::FloatRect& caretRect) {
    if (id == 0 || id != id_) {
        return;
    }
    const sf::Vector2u size = window_.getSize();
    onMain(^{
      if (client_ == nil) {
          return;
      }
      if (!client_->textState.preedit.empty() && state.preedit.empty()) {
          const Sink sink = client_->eventSink;
          client_->eventSink = {};
          [client_->textContext discardMarkedText];
          client_->eventSink = sink;
      }
      client_->textState = state;
      client_->caretRectangle = caretRect;
      client_->surfaceSize = size;
      [client_->textContext invalidateCharacterCoordinates];
    });
}

void MacTextInputHostImpl::end(ludork::engine::text_input::SessionId id) {
    if (id == 0 || id != id_) {
        return;
    }
    id_ = 0;
    const sf::WindowHandle handle = window_.getNativeHandle();
    onMain(^{
      @autoreleasepool {
          if (eventMonitor_ != nil) {
              [NSEvent removeMonitor:eventMonitor_];
              [eventMonitor_ release];
              eventMonitor_ = nil;
          }
          if (client_ != nil) {
              client_->eventSink = {};
              [client_->textContext discardMarkedText];
              [client_->textContext deactivate];
              if ([nativeWindow_ firstResponder] == client_) {
                  [nativeWindow_ makeFirstResponder:previousResponder_];
              }
              [client_ removeFromSuperview];
              [client_->textContext release];
              client_->textContext = nil;
              [client_ release];
              client_ = nil;
          }
          [previousResponder_ release];
          previousResponder_ = nil;
          [nativeWindow_ release];
          nativeWindow_ = nil;
          setNativeInputMethodDisabled(handle, true);
      }
    });
}

std::shared_ptr<ludork::engine::text_input::TextInputHost>
createDesktopTextInputHost(sf::WindowBase& window) {
    return std::make_shared<MacTextInputHostImpl>(window);
}

}  // namespace ludork::global
