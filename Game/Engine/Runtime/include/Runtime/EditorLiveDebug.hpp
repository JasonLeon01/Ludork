#pragma once

#include <LudorkRuntimeBinding/Annotations.hpp>
#include <Runtime/RuntimeIdentity.hpp>
#include <RuntimeApi.hpp>

BIND_CLASS(metadata = false)
class LUDORK_RUNTIME_API EditorLiveDebug {
public:
    /// Install the current scene's editor request handler. The handler accepts
    /// one request table and returns one JSON-serializable response table.
    BIND_METHOD(metadata = false, parameter_types = {function})
    static void install(const RuntimeIdentityPtr& handler);

    /// Release the editor request handler when its scene is no longer active.
    BIND_METHOD(metadata = false)
    static void uninstall();
};
