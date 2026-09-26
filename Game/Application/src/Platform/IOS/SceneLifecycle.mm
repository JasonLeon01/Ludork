#include "SceneLifecycle.hpp"

#include "LudorkSceneDelegate.hpp"

#include <CoreFoundation/CoreFoundation.h>

namespace ludork::application::detail {

void waitForActiveIosScene() {
    while (true) {
        @autoreleasepool {
            for (UIScene* scene in UIApplication.sharedApplication
                     .connectedScenes) {
                if ([scene isKindOfClass:UIWindowScene.class] &&
                    scene.activationState ==
                        UISceneActivationStateForegroundActive &&
                    [scene.delegate isKindOfClass:LudorkSceneDelegate.class] &&
                    ((LudorkSceneDelegate*)scene.delegate).window != nil) {
                    return;
                }
            }
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
        }
    }
}

}  // namespace ludork::application::detail
