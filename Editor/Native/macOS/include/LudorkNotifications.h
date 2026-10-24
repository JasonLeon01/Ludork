#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum LudorkNotificationStatus {
    LudorkNotificationActivated = 0,
    LudorkNotificationDismissed = 1,
    LudorkNotificationDenied = 2,
    LudorkNotificationFailed = 3,
};

typedef void (*LudorkNotificationCallback)(void* context, uint32_t id,
                                           int32_t status, const char* error);

// Calls and callbacks run on the main thread. Callback strings are valid only
// during the callback.
__attribute__((visibility("default"))) void* ludork_notifications_create(
    LudorkNotificationCallback callback, void* context);
__attribute__((visibility("default"))) void ludork_notifications_show(
    void* handle, uint32_t id, const char* title, const char* body);
__attribute__((visibility("default"))) void ludork_notifications_close(
    void* handle, uint32_t id);
// Returning from destroy guarantees that no further managed callbacks will be
// made.
__attribute__((visibility("default"))) void ludork_notifications_destroy(
    void* handle);

#ifdef __cplusplus
}
#endif
