#include "LudorkNotificationCenterImpl.hpp"

@implementation LudorkNotificationCenterImpl

- (instancetype)initWithBundleIdentifier:(NSString*)bundleIdentifier
                                callback:(LudorkNotificationCallback)callback
                                 context:(void*)context {
    self = [super init];
    if (self != nil) {
        _center = UNUserNotificationCenter.currentNotificationCenter;
        _requests = [NSMutableDictionary dictionary];
        _identifierPrefix =
            [NSString stringWithFormat:@"%@.%@", bundleIdentifier,
                                       NSUUID.UUID.UUIDString];
        _categoryIdentifier =
            [bundleIdentifier stringByAppendingString:@".completed-task"];
        _callback = callback;
        _context = context;
        UNNotificationCategory* category = [UNNotificationCategory
            categoryWithIdentifier:_categoryIdentifier
                           actions:@[]
                 intentIdentifiers:@[]
                           options:
                               UNNotificationCategoryOptionCustomDismissAction];
        [_center setNotificationCategories:[NSSet setWithObject:category]];
        _center.delegate = self;
    }
    return self;
}

- (void)show:(uint32_t)notificationId
       title:(NSString*)title
        body:(NSString*)body {
    if (_disposed) {
        return;
    }
    [self close:notificationId];
    @try {
        UNMutableNotificationContent* content =
            [[UNMutableNotificationContent alloc] init];
        content.title = title;
        content.body = body;
        content.categoryIdentifier = _categoryIdentifier;
        NSString* identifier =
            [NSString stringWithFormat:@"%@.%u.%@", _identifierPrefix,
                                       notificationId, NSUUID.UUID.UUIDString];
        UNNotificationRequest* request =
            [UNNotificationRequest requestWithIdentifier:identifier
                                                 content:content
                                                 trigger:nil];
        _requests[@(notificationId)] = request;
        [_center
            requestAuthorizationWithOptions:UNAuthorizationOptionAlert
                          completionHandler:^(BOOL granted, NSError* error) {
                            dispatch_async(dispatch_get_main_queue(), ^{
                              if (self->_disposed ||
                                  self->_requests[@(notificationId)] !=
                                      request) {
                                  return;
                              }
                              if (error != nil) {
                                  [self complete:notificationId
                                          status:LudorkNotificationFailed
                                           error:error.localizedDescription];
                              } else if (!granted) {
                                  [self complete:notificationId
                                          status:LudorkNotificationDenied
                                           error:nil];
                              } else {
                                  [self submit:request
                                      notificationId:notificationId];
                              }
                            });
                          }];
    } @catch (NSException* exception) {
        [self close:notificationId];
        [self notify:notificationId
              status:LudorkNotificationFailed
               error:exception.reason];
    }
}

- (void)submit:(UNNotificationRequest*)request
    notificationId:(uint32_t)notificationId {
    @try {
        UNUserNotificationCenter* center = _center;
        [center addNotificationRequest:request
                 withCompletionHandler:^(NSError* error) {
                   dispatch_async(dispatch_get_main_queue(), ^{
                     if (self->_disposed ||
                         self->_requests[@(notificationId)] != request) {
                         [self revoke:@[ request.identifier ]];
                     } else if (error != nil) {
                         [self complete:notificationId
                                 status:LudorkNotificationFailed
                                  error:error.localizedDescription];
                     }
                   });
                 }];
    } @catch (NSException* exception) {
        [self complete:notificationId
                status:LudorkNotificationFailed
                 error:exception.reason];
    }
}

- (void)close:(uint32_t)notificationId {
    UNNotificationRequest* request = _requests[@(notificationId)];
    if (request == nil) {
        return;
    }
    [_requests removeObjectForKey:@(notificationId)];
    [self revoke:@[ request.identifier ]];
}

- (void)revoke:(NSArray<NSString*>*)identifiers {
    @try {
        [_center removePendingNotificationRequestsWithIdentifiers:identifiers];
        [_center removeDeliveredNotificationsWithIdentifiers:identifiers];
    } @catch (NSException* exception) {
        NSLog(@"Could not remove Ludork notifications: %@", exception.reason);
    }
}

- (void)complete:(uint32_t)notificationId
          status:(int32_t)status
           error:(NSString*)error {
    [self close:notificationId];
    [self notify:notificationId status:status error:error];
}

- (void)notify:(uint32_t)notificationId
        status:(int32_t)status
         error:(NSString*)error {
    if (!_disposed && _callback != nullptr) {
        _callback(_context, notificationId, status, error.UTF8String);
    }
}

- (NSNumber*)keyForIdentifier:(NSString*)identifier {
    for (NSNumber* key in _requests) {
        if ([_requests[key].identifier isEqualToString:identifier]) {
            return key;
        }
    }
    return nil;
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:
             (void (^)(UNNotificationPresentationOptions options))
                 completionHandler {
    dispatch_async(dispatch_get_main_queue(), ^{
      BOOL active =
          !self->_disposed &&
          [self keyForIdentifier:notification.request.identifier] != nil;
      completionHandler(active ? UNNotificationPresentationOptionList |
                                     UNNotificationPresentationOptionBanner
                               : UNNotificationPresentationOptionNone);
    });
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
    didReceiveNotificationResponse:(UNNotificationResponse*)response
             withCompletionHandler:(void (^)(void))completionHandler {
    dispatch_async(dispatch_get_main_queue(), ^{
      completionHandler();
      NSNumber* key =
          [self keyForIdentifier:response.notification.request.identifier];
      if (self->_disposed || key == nil) {
          return;
      }
      int32_t status =
          [response.actionIdentifier
              isEqualToString:UNNotificationDefaultActionIdentifier]
              ? LudorkNotificationActivated
              : LudorkNotificationDismissed;
      [self complete:key.unsignedIntValue status:status error:nil];
    });
}

- (void)dispose {
    if (_disposed) {
        return;
    }
    _disposed = YES;
    _callback = nullptr;
    _context = nullptr;
    NSArray<NSString*>* identifiers =
        [_requests.allValues valueForKey:@"identifier"];
    [_requests removeAllObjects];
    [self revoke:identifiers];
    if (_center.delegate == self) {
        _center.delegate = nil;
    }
}

@end
