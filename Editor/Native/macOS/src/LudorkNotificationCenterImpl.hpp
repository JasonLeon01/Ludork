#pragma once

#include "LudorkNotifications.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

@interface LudorkNotificationCenterImpl
    : NSObject <UNUserNotificationCenterDelegate> {
@private
    UNUserNotificationCenter* _center;
    NSMutableDictionary<NSNumber*, UNNotificationRequest*>* _requests;
    NSString* _identifierPrefix;
    NSString* _categoryIdentifier;
    LudorkNotificationCallback _callback;
    void* _context;
    BOOL _disposed;
}

- (instancetype)initWithBundleIdentifier:(NSString*)bundleIdentifier
                                callback:(LudorkNotificationCallback)callback
                                 context:(void*)context;
- (void)show:(uint32_t)notificationId
       title:(NSString*)title
        body:(NSString*)body;
- (void)close:(uint32_t)notificationId;
- (void)dispose;

@end
