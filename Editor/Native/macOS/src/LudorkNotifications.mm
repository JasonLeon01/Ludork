#include "LudorkNotifications.h"

#include "LudorkNotificationCenterImpl.hpp"

void* ludork_notifications_create(LudorkNotificationCallback callback,
                                  void* context) {
    @autoreleasepool {
        if (callback == nullptr || ![NSThread isMainThread]) {
            return nullptr;
        }
        NSString* bundleIdentifier = NSBundle.mainBundle.bundleIdentifier;
        if (bundleIdentifier.length == 0) {
            return nullptr;
        }
        @try {
            LudorkNotificationCenterImpl* center =
                [[LudorkNotificationCenterImpl alloc]
                    initWithBundleIdentifier:bundleIdentifier
                                    callback:callback
                                     context:context];
            return (__bridge_retained void*)center;
        } @catch (NSException* exception) {
            NSLog(@"Could not initialize Ludork notifications: %@",
                  exception.reason);
            return nullptr;
        }
    }
}

void ludork_notifications_show(void* handle, uint32_t id, const char* title,
                               const char* body) {
    @autoreleasepool {
        LudorkNotificationCenterImpl* center =
            (__bridge LudorkNotificationCenterImpl*)handle;
        [center
             show:id
            title:title == nullptr ? @"" : [NSString stringWithUTF8String:title]
             body:body == nullptr ? @"" : [NSString stringWithUTF8String:body]];
    }
}

void ludork_notifications_close(void* handle, uint32_t id) {
    @autoreleasepool {
        LudorkNotificationCenterImpl* center =
            (__bridge LudorkNotificationCenterImpl*)handle;
        [center close:id];
    }
}

void ludork_notifications_destroy(void* handle) {
    @autoreleasepool {
        LudorkNotificationCenterImpl* center =
            (__bridge_transfer LudorkNotificationCenterImpl*)handle;
        [center dispose];
    }
}
