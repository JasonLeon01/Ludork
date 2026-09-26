#include "NativeDisplay.hpp"

#import <UIKit/UIKit.h>

#include <stdexcept>

namespace ludork::global {

void attachIosWindowScene(sf::WindowHandle windowHandle) {
    UIWindow* window = (__bridge UIWindow*)windowHandle;
    if (window == nil || ![NSThread isMainThread]) {
        throw std::runtime_error(
            "The iOS game window must be attached on the main thread");
    }
    UIWindowScene* windowScene = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:UIWindowScene.class] &&
            scene.activationState == UISceneActivationStateForegroundActive &&
            [scene.session.role
                isEqualToString:UIWindowSceneSessionRoleApplication]) {
            if (windowScene != nil) {
                throw std::runtime_error(
                    "The iOS game requires exactly one active window scene");
            }
            windowScene = (UIWindowScene*)scene;
        }
    }
    if (windowScene == nil) {
        throw std::runtime_error("The iOS game window scene is not active");
    }
    id<UIWindowSceneDelegate> delegate =
        (id<UIWindowSceneDelegate>)windowScene.delegate;
    if (![delegate respondsToSelector:@selector(window)] ||
        ![delegate respondsToSelector:@selector(setWindow:)]) {
        throw std::runtime_error(
            "The iOS window scene delegate must own the game window");
    }
    UIWindow* previousWindow = delegate.window;
    if (previousWindow != window) {
        previousWindow.hidden = YES;
    }
    window.windowScene = windowScene;
    window.frame = windowScene.coordinateSpace.bounds;
    delegate.window = window;
    [window makeKeyAndVisible];
    [window layoutIfNeeded];
    [window.rootViewController.view layoutIfNeeded];
}

}  // namespace ludork::global
