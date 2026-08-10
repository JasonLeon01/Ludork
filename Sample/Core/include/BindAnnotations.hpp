#pragma once

// These declarations are intentionally empty at C++ compile time. The Core
// bindgen reads the annotations from public headers and emits the sol2
// registration source.
#define BIND_CLASS(...)
#define BIND_FUNCTION(...)
#define BIND_FUNCTION_GROUP(...)
#define BIND_METHOD(...)
#define BIND_PROPERTY(...)
#define BIND_INIT(...)
#define BIND_IGNORE(...)
#define BIND_REGISTER_EVENT(...)
#define BIND_INVALID_VARS(...)
#define BIND_RECT_RANGE_VARS(...)
#define BIND_LOOP_NODE(...)
#define BIND_INJECT(...)
#define BIND_MODULE_PROPERTY(...)
#define BIND_CLASS_PROPERTY(...)
#define BIND_LUA_REVERSE(...)
#define BIND_LUA_HELPER(...)
#define BIND_MODULE_INIT(...)
#define BIND_UI_CONTROL(...)
#define BIND_DYNAMIC_VALUE_TYPE() using LuaBindingDynamicValueTag = void
#define BIND_OPAQUE_IDENTITY_TYPE() using LuaBindingOpaqueIdentityTag = void
