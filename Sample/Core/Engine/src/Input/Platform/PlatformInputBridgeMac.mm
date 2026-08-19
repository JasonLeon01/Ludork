#include "PlatformInputBridge.hpp"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <cmath>
#include <mutex>
#include <vector>

namespace ludork::engine::platform_input {
namespace {

struct PendingScrollEvent {
    NSWindow* window = nil;
    NSPoint location = NSZeroPoint;
    NSPoint delta = NSZeroPoint;
    bool precise = false;
};

id scrollMonitor = nil;
std::mutex scrollMutex;
std::vector<PendingScrollEvent> pendingScrollEvents;

NSView* viewForHandle(sf::WindowHandle handle) {
    id nativeHandle = static_cast<id>(handle);
    if ([nativeHandle isKindOfClass:[NSWindow class]]) {
        return [static_cast<NSWindow*>(nativeHandle) contentView];
    }
    if ([nativeHandle isKindOfClass:[NSView class]]) {
        return static_cast<NSView*>(nativeHandle);
    }
    return nil;
}

NSWindow* windowForHandle(sf::WindowHandle handle) {
    id nativeHandle = static_cast<id>(handle);
    if ([nativeHandle isKindOfClass:[NSWindow class]]) {
        return static_cast<NSWindow*>(nativeHandle);
    }
    if ([nativeHandle isKindOfClass:[NSView class]]) {
        return [static_cast<NSView*>(nativeHandle) window];
    }
    return nil;
}

sf::Vector2f coordinateScale(const sf::WindowBase& window, NSView* view) {
    const NSRect bounds = [view bounds];
    const sf::Vector2u size = window.getSize();
    const float scaleX = bounds.size.width > 0.0
                             ? static_cast<float>(size.x / bounds.size.width)
                             : 0.0f;
    const float scaleY = bounds.size.height > 0.0
                             ? static_cast<float>(size.y / bounds.size.height)
                             : 0.0f;
    return {scaleX, scaleY};
}

sf::Vector2i pixelPosition(const sf::WindowBase& window, NSView* view,
                           const NSPoint& windowPosition) {
    const NSRect bounds = [view bounds];
    const NSPoint viewPosition = [view convertPoint:windowPosition
                                           fromView:nil];
    const sf::Vector2f scale = coordinateScale(window, view);
    const CGFloat top = [view isFlipped] ? viewPosition.y - NSMinY(bounds)
                                         : NSMaxY(bounds) - viewPosition.y;
    return {static_cast<int>(
                std::lround((viewPosition.x - NSMinX(bounds)) * scale.x)),
            static_cast<int>(std::lround(top * scale.y))};
}

void enqueueScrollEvent(NSEvent* event) {
    const bool precise = [event hasPreciseScrollingDeltas];
    const CGFloat deltaX = precise ? [event scrollingDeltaX] : [event deltaX];
    const CGFloat deltaY = precise ? [event scrollingDeltaY] : [event deltaY];
    if (deltaX == 0.0 && deltaY == 0.0) {
        return;
    }
    const PendingScrollEvent pending{
        [event window],
        [event locationInWindow],
        NSMakePoint(deltaX, deltaY),
        precise,
    };
    const std::lock_guard<std::mutex> lock(scrollMutex);
    pendingScrollEvents.push_back(pending);
}

}  // namespace

void initialize() {
    @autoreleasepool {
        if (scrollMonitor != nil) {
            return;
        }
        NSEvent* (^handler)(NSEvent*) = ^NSEvent*(NSEvent* event) {
          enqueueScrollEvent(event);
          return event;
        };
        scrollMonitor =
            [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
                                                  handler:handler];
    }
}

void shutdown() noexcept {
    @autoreleasepool {
        if (scrollMonitor != nil) {
            [NSEvent removeMonitor:scrollMonitor];
            scrollMonitor = nil;
        }
        const std::lock_guard<std::mutex> lock(scrollMutex);
        pendingScrollEvents.clear();
    }
}

bool setMousePosition(sf::WindowBase& window, const sf::Vector2i& position) {
    @autoreleasepool {
        NSView* view = viewForHandle(window.getNativeHandle());
        NSWindow* nativeWindow = windowForHandle(window.getNativeHandle());
        if (view == nil || nativeWindow == nil) {
            return false;
        }
        const sf::Vector2f scale = coordinateScale(window, view);
        if (scale.x <= 0.0f || scale.y <= 0.0f) {
            return false;
        }
        const NSRect bounds = [view bounds];
        const CGFloat x = NSMinX(bounds) + position.x / scale.x;
        const CGFloat top = position.y / scale.y;
        const CGFloat y =
            [view isFlipped] ? NSMinY(bounds) + top : NSMaxY(bounds) - top;
        const NSPoint windowPoint = [view convertPoint:NSMakePoint(x, y)
                                                toView:nil];
        const NSPoint screenPoint =
            [nativeWindow convertPointToScreen:windowPoint];
        NSScreen* screen = nil;
        for (NSScreen* candidate in [NSScreen screens]) {
            if (NSPointInRect(screenPoint, [candidate frame])) {
                screen = candidate;
                break;
            }
        }
        if (screen == nil) {
            screen = [nativeWindow screen];
        }
        if (screen == nil) {
            return false;
        }
        NSNumber* displayNumber =
            [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
        if (displayNumber == nil) {
            return false;
        }
        const NSRect screenFrame = [screen frame];
        const CGPoint displayPoint =
            CGPointMake(screenPoint.x - NSMinX(screenFrame),
                        NSMaxY(screenFrame) - screenPoint.y);
        const CGError error = CGDisplayMoveCursorToPoint(
            static_cast<CGDirectDisplayID>([displayNumber unsignedIntValue]),
            displayPoint);
        return error == kCGErrorSuccess;
    }
}

bool isScrollCaptureAvailable() {
    return scrollMonitor != nil;
}

std::vector<ScrollEvent> consumeScrollEvents(sf::WindowBase& window) {
    @autoreleasepool {
        NSView* view = viewForHandle(window.getNativeHandle());
        NSWindow* nativeWindow = windowForHandle(window.getNativeHandle());
        if (view == nil || nativeWindow == nil) {
            return {};
        }
        std::vector<PendingScrollEvent> pending;
        {
            const std::lock_guard<std::mutex> lock(scrollMutex);
            pending.swap(pendingScrollEvents);
        }
        const sf::Vector2f scale = coordinateScale(window, view);
        std::vector<ScrollEvent> events;
        events.reserve(pending.size());
        for (const PendingScrollEvent& event : pending) {
            if (event.window != nativeWindow) {
                continue;
            }
            sf::Vector2f delta{static_cast<float>(event.delta.x),
                               static_cast<float>(event.delta.y)};
            if (event.precise) {
                delta.x *= scale.x;
                delta.y *= scale.y;
            }
            events.push_back({delta,
                              pixelPosition(window, view, event.location),
                              event.precise});
        }
        return events;
    }
}

}  // namespace ludork::engine::platform_input
