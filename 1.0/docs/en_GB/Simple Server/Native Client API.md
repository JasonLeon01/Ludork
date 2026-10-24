# Native Client API

Configure the URL and project key through [Deployment and Editor Settings](<Deployment and Editor Settings.md>). Game code uses `Global.Net` for explicit data snapshots, void RPCs and named server events. C++ owns the WebSocket connection, value encoding and delivery on the game logic thread; Lua never supplies connection credentials. `GlobalCore.Server.CheckConnectionAsync` remains the separate HTTP health probe.

## Disabled server integration

When server integration is disabled in the build, each server interaction prints a diagnostic and returns immediately. It does not connect, send or queue requests, create an asynchronous operation, register listeners or polling, copy request parameters, change proxy state, invoke callbacks or publish error events. The APIs remain callable, with these return values:

| Call | Disabled return value |
| --- | --- |
| `Net.GetProxy`, `proxy:Sync`, `Server.CheckConnectionAsync`, native `NetTransport.GetSnapshotAsync` | `nil` |
| `module.Subscribe`, `module.SubscribeError` | `0`, without a listener |
| `module.Unsubscribe` | `false` |
| `module.CallRpcFunction` | No values |

Check that an operation is not nil before waiting on it or calling its methods. A disabled call does not produce a failure result with code `Disabled` or a `Disconnected` health result. `Net.DefineModule`, constructing a proxy from local data, `proxy:GetValue` and native `NetTransport.CloneValue` remain available as local operations. The request, callback and connection behaviour below applies when integration is enabled.

## Define a network module and proxy

A proxy is a local Lua object containing one server snapshot. Its getters read that snapshot without network I/O. Define a subclass of `Global.NetProxy` in its own module, then register it under the same network-module name as the server. The corresponding server implementation is described in [Server Lua modules](<HTTP Protocol.md#server-lua-modules>).

```lua
-- Scripts/Source/Net/ProfileProxy.lua
local NetProxy = require("Global.NetProxy")

local ProfileProxy = {}

function ProfileProxy:GetName()
    return self:GetValue("name")
end

return class(ProfileProxy, NetProxy)
```

```lua
-- Scripts/Source/Net/Profile.lua
local Net = require("Global.Net")
local ProfileProxy = require("Source.Net.ProfileProxy")

return Net.DefineModule("Profile", ProfileProxy)
```

`Net.DefineModule(name, proxyClass)` returns the module handle used for RPCs and subscriptions. The class must derive from `Global.NetProxy`; duplicate names are rejected. Defining a module does not connect. Require its definition before requesting a proxy. A subclass that overrides `init(moduleName, params, snapshot)` must call the base initializer with those arguments; otherwise the inherited initializer is used. See [Ludork classes](<../Getting Started/Ludork Lua Advanced.md#ludork-classes>) for class construction and mirrored declarations.

Module, RPC and event names are case-sensitive and use 1–128 ASCII letters, digits, dots, underscores or hyphens. RPC function names refer to the server module's `Rpc` table, not proxy getter names.

Public Net, proxy, getter and RPC functions use PascalCase, such as `GetProxy`, `Sync`, `GetName` and `SetName`. The class constructor keeps the existing `init` convention.

## Fetch and refresh snapshots

```lua
local Net = require("Global.Net")
local Profile = require("Source.Net.Profile")
local Logging = require("Global.Utils.Logging")

local operation = Net.GetProxy("Profile", {account = "player-1"}, function(result)
    if not result.ok then
        Logging.warning("Profile failed: %s", result.message)
        return
    end
    local proxy = assert(result.proxy)
    Logging.info("Name: %s", tostring(proxy:GetValue("name")))
    proxy:Sync(function(refreshed)
        if refreshed.ok then
            assert(refreshed.proxy == proxy)
            Logging.info("Refreshed name: %s", tostring(proxy:GetValue("name")))
        end
    end)
end)
```

`Net.GetProxy(moduleName, params, callback)` always sends a new snapshot request and constructs a new proxy on success, even when the name and parameters match an earlier request. Parameters are copied when requested; changing the caller's table later does not alter this proxy's query. Separate proxies and their snapshots are independent.

`proxy:Sync(callback)` requests the same module with its captured parameters and refreshes the **existing object**. It replaces the complete snapshot only after a successful response. Pending, failed or cancelled syncs leave the previous snapshot readable. Multiple syncs on one proxy are serialized in call order, with one request per call; they are not combined. To change query parameters, call `GetProxy` for a new object.

`proxy:GetValue(key)` returns a snapshot field. Table values are independent copies, including nested tables, so modifying a getter result cannot write the snapshot or server. Business getters such as `GetName` should use this method rather than accessing private fields. Neither an RPC nor a server event refreshes any proxy automatically.

When integration is enabled, both fetch methods require a callback and return `Engine.AsyncOperation`. Their callback receives an ordinary Lua result table:

| Field | Meaning |
| --- | --- |
| `ok` | Whether a snapshot was obtained and installed. |
| `code` | String error code; `"None"` on success. |
| `message` | Diagnostic text; empty on success. |
| `proxy` | The new proxy for `GetProxy`, or the same object for successful `Sync`; nil on failure. |
| `retryAfterSeconds` | Optional whole-second delay for a rate-limited request. |

The operation stores this result before invoking the callback; `operation:getResult()` returns the **same table** passed to it. Failure completes normally with `ok == false`. Explicit cancellation suppresses proxy construction or replacement and the callback. It cannot undo work already accepted by the server. Closing the game session cancels pending work and removes its listeners.

Requests have a **ten-second deadline from submission**, including connection and queue time. A response processed after that deadline is rejected even if the logic thread was paused when it arrived. The `Global.Net` wrappers initialize latent-operation updates, but a running game update loop must still pump them; callbacks do not run by themselves in a bare script. See [asynchronous operation updates](<../Lua and Blueprint Scripting/Lua Runtime and Modules.md#asynchronous-operation-updates>).

## Call RPCs and receive events

```lua
local Profile = require("Source.Net.Profile")
local Logging = require("Global.Utils.Logging")

local errorToken = Profile.SubscribeError(function(error)
    Logging.warning("Profile RPC/connection failed (%s): %s", error.code, error.message)
end)

local eventToken = Profile.Subscribe("NameChanged", function(account)
    Logging.info("Name changed for %s", account)
    -- Fetch or Sync here if this screen needs a newer snapshot.
end)

Profile.CallRpcFunction("SetName", "player-1", "Ada")
```

`module.CallRpcFunction(functionName, ...)` executes the identically named server RPC with positional arguments. It returns **no values and no operation handle**. A table passed as an argument is one argument, not an argument list. Nil gaps and trailing nil arguments retain their positions. Business replies use named events; the internal success acknowledgment is not a function return value.

`module.Subscribe(eventName, handler)` and `module.SubscribeError(handler)` return integer listener tokens. `module.Unsubscribe(token)` returns whether it removed a listener from this module. Subscribe before issuing the RPC whose event matters. Listeners remain active until removed or the game session ends; they also remain registered across connection recovery. Remove them later, when their owning screen or object no longer needs them:

```lua
Profile.Unsubscribe(eventToken)
Profile.Unsubscribe(errorToken)
```

Events contain the server's positional arguments and run on the logic thread. They are online-only: disconnected clients receive no backlog. Event and error subscriptions use `EventBus`: a throwing listener is logged and other listeners continue. A snapshot completion callback's error propagates to the host after the operation has stored its result. These application failures are not converted into malformed-network errors.

A module error contains `code`, `message`, `module`, and optional `functionName`, `requestId` and `retryAfterSeconds`. RPC failures identify the function and request where available; a connection-level error may omit them. Snapshot failures are delivered through their result callbacks. Invalid local arguments, unsupported values and missing callbacks are programming errors rejected at the call boundary.

All modules share one connection per game session. The first fetch, RPC or subscription starts it, so a client that only subscribes can receive events. Connection loss fails pending requests. The transport reconnects automatically after ordinary connection failures, without replaying RPCs, reissuing old snapshot requests or syncing existing proxies. Authentication rejection stops reconnect attempts for that session. Correct the compiled configuration and start a new session.

An RPC can commit a write before a timeout or disconnect is observed. Read a fresh snapshot before deciding whether the business action should be attempted again; there is no automatic rollback or exactly-once replay mechanism.

## Lua values and errors

Parameters and snapshot data use ordinary Lua nil, booleans, finite numbers, byte strings and acyclic tables. Mixed and sparse boolean/number/string keys and signed 64-bit integers are preserved. Functions, threads, userdata, class/native objects, metatables, cycles and table keys are rejected. For the exact value and size rules, see [Lua data and wire encoding](<HTTP Protocol.md#lua-data-and-wire-encoding>); these values do not use the `RuntimeData` JSON conversion contract.

Network results use **string codes**, including `InvalidArgument`, `AuthenticationFailed`, `Timeout`, `ConnectionFailed`, `InvalidResponse`, `QueueFull`, `StorageFailure`, `ServerFailure` and `RateLimited`. Server business handlers may return additional valid codes. Use the code for decisions and the message for diagnostics. `retryAfterSeconds`, when present, is the minimum whole-second delay to respect before another attempt.

## Supplied Storage module

Require `Global.Net.Storage` to register the built-in client module and its `Global.Net.StorageProxy` class. The server entry must register `Modules.Storage` under `"Storage"`. Fetch its snapshots with `Net.GetProxy("Storage", params, callback)`.

| `params.query` | Proxy getters |
| --- | --- |
| `"account_exists"` | `AccountExists()` |
| `"accounts"` | `GetAccounts()` |
| `"field"` | `IsFound()`, `GetFieldValue()` |
| `"fields"` | `GetEntries()`, `GetNextCursor()` |

Each getter reads only that proxy's query result. Unrelated getters may return nil. Entries have `key` and `value` fields. A nil next cursor ends pagination; pass another cursor in a new `GetProxy` request. Query parameters, account sampling, field order, missing values and storage limits are defined in [Storage services](<HTTP Protocol.md#storage-services>).

Write with `Storage.CallRpcFunction("WriteField", account, category, field, value)`. Nil deletes the field. Success emits `FieldWritten(account, category, field)` to the caller. This supplied RPC alone uses the [configured write interval](<HTTP Protocol.md#write-rate-limits>), normally 180 seconds; custom RPCs do not inherit it. There is no write-token API.

## Network quality

When integration is enabled, the independent `GlobalCore.Server.CheckConnectionAsync(callback)` uses its native result and a three-second HTTP deadline. Custom entry points must initialize `GlobalFunctions.NodeGraph.initLatent()` and run operation updates. Its callback receives `GlobalCore.ServerConnectionResult`; narrow the generated nullable native-object declaration with `assert(result)`.

```lua
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")

GlobalCore.Server.CheckConnectionAsync(function(result)
    assert(result)
    if result.state == GlobalCore.ServerNetworkState.Normal then
        Logging.info("Normal: %s ms", tostring(result.roundTripMs))
    elseif result.state == GlobalCore.ServerNetworkState.Weak then
        Logging.info("Weak: %s ms", tostring(result.roundTripMs))
    else
        Logging.warning("Disconnected: %s", result.message)
    end
end)
```

`Normal` means a valid configured-service health response within 300 ms; `Weak` means more than 300 ms within three seconds; `Disconnected` means no valid response or a failed request. The result also exposes `ok`, `code` as the native `GlobalCore.ServerErrorCode` enum, `message`, and `roundTripMs` when a valid response was received. It returns an `Engine.AsyncOperation` and preserves callback/result identity and cancellation semantics.

The health endpoint does not check the project key, so a healthy result does not establish RPC authentication or overall Internet access. System DNS resolution may remain blocked after cancellation, and session shutdown may wait for it; cancellation does not guarantee a hard DNS or shutdown deadline.
