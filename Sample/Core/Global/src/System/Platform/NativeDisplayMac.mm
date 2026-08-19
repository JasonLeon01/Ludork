#include "NativeDisplay.hpp"

#import <AppKit/AppKit.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ludork::global {
namespace {

NSWindow* windowForHandle(sf::WindowHandle windowHandle) {
    id nativeHandle = static_cast<id>(windowHandle);
    if ([nativeHandle isKindOfClass:[NSWindow class]]) {
        return static_cast<NSWindow*>(nativeHandle);
    }
    if ([nativeHandle isKindOfClass:[NSView class]]) {
        return [static_cast<NSView*>(nativeHandle) window];
    }
    return nil;
}

std::optional<unsigned int> unsignedSize(CGFloat value) noexcept {
    const double numericValue = static_cast<double>(value);
    if (!std::isfinite(numericValue) || numericValue <= 0.0) {
        return std::nullopt;
    }
    const double roundedValue = std::round(numericValue);
    if (roundedValue <= 0.0 ||
        roundedValue >
            static_cast<double>(std::numeric_limits<unsigned int>::max())) {
        return std::nullopt;
    }
    return static_cast<unsigned int>(roundedValue);
}

std::optional<sf::Vector2u> maximumClientSizeForScreen(
    NSScreen* screen) noexcept {
    if (screen == nil) {
        return std::nullopt;
    }
    const NSRect visibleFrame = [screen visibleFrame];
    const NSWindowStyleMask style =
        NSWindowStyleMaskTitled | NSWindowStyleMaskMiniaturizable |
        NSWindowStyleMaskResizable | NSWindowStyleMaskClosable;
    const NSRect contentRect = [NSWindow
        contentRectForFrameRect:NSMakeRect(0.0, 0.0, visibleFrame.size.width,
                                           visibleFrame.size.height)
                      styleMask:style];
    const std::optional<unsigned int> width =
        unsignedSize(std::floor(contentRect.size.width));
    const std::optional<unsigned int> height =
        unsignedSize(std::floor(contentRect.size.height));
    if (!width.has_value() || !height.has_value()) {
        return std::nullopt;
    }
    return sf::Vector2u(*width, *height);
}

}  // namespace

std::optional<sf::Vector2u> getMaximumWindowedClientSize(
    sf::WindowHandle windowHandle) noexcept {
    @autoreleasepool {
        NSWindow* window = windowForHandle(windowHandle);
        NSScreen* screen =
            window != nil ? [window screen] : [NSScreen mainScreen];
        return maximumClientSizeForScreen(screen);
    }
}

std::optional<sf::Vector2u> getWindowedClientSize(
    sf::WindowHandle windowHandle) noexcept {
    @autoreleasepool {
        NSWindow* window = windowForHandle(windowHandle);
        NSView* contentView = window != nil ? [window contentView] : nil;
        if (contentView == nil) {
            return std::nullopt;
        }
        const NSSize size = [contentView frame].size;
        const std::optional<unsigned int> width = unsignedSize(size.width);
        const std::optional<unsigned int> height = unsignedSize(size.height);
        if (!width.has_value() || !height.has_value()) {
            return std::nullopt;
        }
        return sf::Vector2u(*width, *height);
    }
}

std::optional<WindowedFramePlacement> getWindowedFramePlacement(
    sf::WindowHandle windowHandle) noexcept {
    @autoreleasepool {
        NSWindow* window = windowForHandle(windowHandle);
        NSScreen* screen = window != nil ? [window screen] : nil;
        if (window == nil || screen == nil) {
            return std::nullopt;
        }
        const NSRect frame = [window frame];
        const NSRect screenFrame = [screen frame];
        const std::optional<unsigned int> screenWidth =
            unsignedSize(screenFrame.size.width);
        const std::optional<unsigned int> screenHeight =
            unsignedSize(screenFrame.size.height);
        if (!screenWidth.has_value() || !screenHeight.has_value()) {
            return std::nullopt;
        }
        return WindowedFramePlacement{
            {static_cast<int>(std::round(NSMinX(frame))),
             static_cast<int>(std::round(NSMaxY(frame)))},
            {static_cast<int>(std::round(NSMinX(screenFrame))),
             static_cast<int>(std::round(NSMinY(screenFrame)))},
            {*screenWidth, *screenHeight}};
    }
}

void setWindowedFramePlacement(
    sf::WindowHandle windowHandle,
    const WindowedFramePlacement& placement) noexcept {
    @autoreleasepool {
        NSWindow* window = windowForHandle(windowHandle);
        if (window == nil) {
            return;
        }
        NSScreen* screen = nil;
        for (NSScreen* candidate in [NSScreen screens]) {
            const NSRect frame = [candidate frame];
            const bool sameOrigin =
                static_cast<int>(std::round(NSMinX(frame))) ==
                    placement.screenOrigin.x &&
                static_cast<int>(std::round(NSMinY(frame))) ==
                    placement.screenOrigin.y;
            const std::optional<unsigned int> width =
                unsignedSize(frame.size.width);
            const std::optional<unsigned int> height =
                unsignedSize(frame.size.height);
            if (sameOrigin && width.has_value() && height.has_value() &&
                *width == placement.screenSize.x &&
                *height == placement.screenSize.y) {
                screen = candidate;
                break;
            }
        }
        if (screen == nil) {
            screen = [window screen];
        }
        if (screen == nil) {
            screen = [NSScreen mainScreen];
        }
        if (screen == nil) {
            return;
        }
        const NSRect visibleFrame = [screen visibleFrame];
        const NSRect frame = [window frame];
        const CGFloat x =
            frame.size.width <= visibleFrame.size.width
                ? std::clamp(static_cast<CGFloat>(placement.topLeft.x),
                             NSMinX(visibleFrame),
                             NSMaxX(visibleFrame) - frame.size.width)
                : NSMinX(visibleFrame);
        const CGFloat top =
            frame.size.height <= visibleFrame.size.height
                ? std::clamp(static_cast<CGFloat>(placement.topLeft.y),
                             NSMinY(visibleFrame) + frame.size.height,
                             NSMaxY(visibleFrame))
                : NSMaxY(visibleFrame);
        [window setFrameTopLeftPoint:NSMakePoint(x, top)];
    }
}

}  // namespace ludork::global
