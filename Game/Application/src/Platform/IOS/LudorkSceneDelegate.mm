#include "LudorkSceneDelegate.hpp"

#import <objc/runtime.h>

namespace {

char disconnectedWindowKey;

}  // namespace

@implementation LudorkSceneDelegate

@synthesize window = window_;

- (void)dealloc {
    [window_ release];
    [super dealloc];
}

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions {
    if (![scene isKindOfClass:UIWindowScene.class]) {
        return;
    }
    UIWindowScene* windowScene = (UIWindowScene*)scene;
    UIWindow* retainedWindow =
        (UIWindow*)objc_getAssociatedObject(session, &disconnectedWindowKey);
    if (retainedWindow != nil) {
        self.window = retainedWindow;
        objc_setAssociatedObject(session, &disconnectedWindowKey, nil,
                                 OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        retainedWindow.windowScene = windowScene;
        retainedWindow.frame = windowScene.coordinateSpace.bounds;
        [retainedWindow makeKeyAndVisible];
        [retainedWindow layoutIfNeeded];
        [retainedWindow.rootViewController.view layoutIfNeeded];
        return;
    }
    UIWindow* window = [[UIWindow alloc] initWithWindowScene:windowScene];
    window.frame = windowScene.coordinateSpace.bounds;
    UIViewController* controller = [[UIViewController alloc] init];
    controller.view.backgroundColor = UIColor.blackColor;
    window.rootViewController = controller;
    [controller release];
    self.window = window;
    [window makeKeyAndVisible];
    [window release];
}

- (void)sceneDidBecomeActive:(UIScene*)scene {
    UIApplication* application = UIApplication.sharedApplication;
    id<UIApplicationDelegate> delegate = application.delegate;
    if ([delegate respondsToSelector:@selector(applicationDidBecomeActive:)]) {
        [delegate applicationDidBecomeActive:application];
    }
}

- (void)sceneWillResignActive:(UIScene*)scene {
    UIApplication* application = UIApplication.sharedApplication;
    id<UIApplicationDelegate> delegate = application.delegate;
    if ([delegate respondsToSelector:@selector(applicationWillResignActive:)]) {
        [delegate applicationWillResignActive:application];
    }
}

- (void)sceneWillEnterForeground:(UIScene*)scene {
    UIApplication* application = UIApplication.sharedApplication;
    id<UIApplicationDelegate> delegate = application.delegate;
    if ([delegate
            respondsToSelector:@selector(applicationWillEnterForeground:)]) {
        [delegate applicationWillEnterForeground:application];
    }
}

- (void)sceneDidEnterBackground:(UIScene*)scene {
    UIApplication* application = UIApplication.sharedApplication;
    id<UIApplicationDelegate> delegate = application.delegate;
    if ([delegate
            respondsToSelector:@selector(applicationDidEnterBackground:)]) {
        [delegate applicationDidEnterBackground:application];
    }
}

- (void)sceneDidDisconnect:(UIScene*)scene {
    [self sceneWillResignActive:scene];
    objc_setAssociatedObject(scene.session, &disconnectedWindowKey, self.window,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    self.window.hidden = YES;
    self.window.windowScene = nil;
    self.window = nil;
}

@end
