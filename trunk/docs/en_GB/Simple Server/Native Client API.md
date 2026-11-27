# Native Client API

Configure the server URL and project key through [Deployment and Editor Settings](<Deployment and Editor Settings.md>). Game code uses `Global.Net` for explicit data snapshots, void RPCs and named server events, while C++ owns the WebSocket connection, wire encoding and logic-thread delivery so Lua never supplies credentials. `GlobalCore.Server.CheckConnectionAsync` provides a separate HTTP health probe.

## Disabled server integration

When server integration is disabled in the build, each server call prints a diagnostic and returns immediately without connecting, sending or queueing requests, creating an asynchronous operation, registering listeners or polling, copying request parameters, modifying proxy state, invoking callbacks or publishing error events. The APIs are callable with these return values:

| Call | Disabled return value |
| --- | --- |
| `Net.GetProxy`, `proxy:Sync`, `Server.CheckConnectionAsync`, native `NetTransport.GetSnapshotAsync` | `nil` |
| `module.Subscribe`, `module.SubscribeError` | `0`, without a listener |
| `module.Unsubscribe` | `false` |
| `module.CallRpcFunction` | No values |

Check that an operation is not `nil` before waiting on it or calling its methods. A disabled call produces neither a `Disabled` failure result nor a `Disconnected` health result. `Net.DefineModule`, constructing a proxy from local data, `proxy:GetValue` and native `NetTransport.CloneValue` work as local operations. The request, callback and connection behaviour below applies when integration is enabled.

## Define a network module and proxy

A proxy is a local Lua object holding one server snapshot, whose getters read that snapshot without network I/O. Define a subclass of `Global.NetProxy` in its own module, then register it under the same network-module name as the server (see [Server Lua modules](<HTTP Protocol.md#server-lua-modules>)).

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

`Net.DefineModule(name, proxyClass)` returns the module handle used for RPCs and subscriptions. `proxyClass` must derive from `Global.NetProxy`, and duplicate names are rejected. Defining a module does not connect; require its definition before requesting a proxy.

A subclass overriding `init(moduleName, params, snapshot)` must call the base initialiser with those arguments; otherwise the inherited initialiser runs. See [Ludork classes](<../Getting Started/Ludork Lua Advanced.md#ludork-classes>) for class construction and mirrored declarations.

Module, RPC and event names are case-sensitive; their character set and length limit are defined in [Server Lua modules](<HTTP Protocol.md#server-lua-modules>). RPC function names refer to the server module's `Rpc` table rather than proxy getter names.

Public Net, proxy, getter and RPC functions use PascalCase (`GetProxy`, `Sync`, `GetName`, `SetName`), while the class constructor uses `init`.

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

- **`Net.GetProxy(moduleName, params, callback)`:** Always sends a new snapshot request and constructs a new proxy on success, even when name and parameters match an earlier call. Parameters are copied on request so later edits to the caller's table do not affect the query; separate proxies and snapshots are independent.
- **`proxy:Sync(callback)`:** Requests the same module with the proxy's captured parameters and refreshes the **existing object**, replacing its full snapshot only on success while pending, failed or cancelled syncs leave the previous snapshot readable. Multiple syncs on one proxy are serialised in call order (one request per call, never coalesced). Call `GetProxy` to query with different parameters.
- **`proxy:GetValue(key)`:** Returns a snapshot field; table values (including nested tables) are returned as independent copies so mutating a getter result cannot alter the snapshot or server. Business getters such as `GetName` should use `GetValue` instead of private fields. Neither RPCs nor server events refresh proxies automatically.

When integration is enabled, both fetch methods require a callback and return `Engine.AsyncOperation`. The callback receives an ordinary Lua result table:

| Field | Meaning |
| --- | --- |
| `ok` | Whether a snapshot was obtained and installed. |
| `code` | String error code; `"None"` on success. |
| `message` | Diagnostic text; empty on success. |
| `proxy` | The new proxy for `GetProxy`, or the same object for successful `Sync`; `nil` on failure. |
| `retryAfterSeconds` | Optional whole-second delay for a rate-limited request. |

The operation stores this result before invoking the callback, so `operation:getResult()` returns the **same table** passed to the callback. Failure completes normally with `ok == false`. Explicit cancellation suppresses proxy construction or replacement and skips the callback without undoing work already accepted by the server. Closing the game session cancels pending work and removes its listeners.

Requests have a **10-second deadline from submission**, including connection and queue time; responses processed after that deadline are rejected even if the logic thread was paused on arrival. `Global.Net` wrappers initialise latent-operation updates, but a running game update loop must pump them—callbacks do not run automatically in a bare script (see [asynchronous operation updates](<../Lua and Blueprint Scripting/Lua Runtime and Modules.md#asynchronous-operation-updates>)).

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

`module.CallRpcFunction(functionName, ...)` invokes the identically named server RPC with positional arguments and returns **no values and no operation handle**. A table argument is passed as one argument rather than unpacked, and `nil` gaps and trailing `nil` arguments keep their positions. Business replies arrive as named events; the internal success acknowledgement is not returned.

`module.Subscribe(eventName, handler)` and `module.SubscribeError(handler)` return integer listener tokens, and `module.Unsubscribe(token)` returns whether it removed a listener from that module. Subscribe before issuing an RPC whose event matters. Listeners stay active until unsubscribed or the game session ends, and persist across connection recovery; unsubscribe them when their owning screen or object no longer needs them:

```lua
Profile.Unsubscribe(eventToken)
Profile.Unsubscribe(errorToken)
```

Events deliver the server's positional arguments on the logic thread and are online-only; disconnected clients receive no backlog.

Event and error subscriptions use `EventBus`: a throwing listener is logged while other listeners continue. A snapshot completion callback's error propagates to the host after the operation stores its result; application errors are not converted into malformed-network errors.

A module error contains `code`, `message`, `module`, and optional `functionName`, `requestId` and `retryAfterSeconds`. RPC failures identify the function and request where available, whereas connection-level errors may omit them. Snapshot failures are delivered through their result callbacks. Invalid local arguments, unsupported values and missing callbacks raise programming errors at the call boundary.

All modules share one connection per game session, opened by the first fetch, RPC or subscription so subscribe-only clients can receive events. Connection loss fails pending requests; the transport reconnects automatically after ordinary failures without replaying RPCs, reissuing snapshots or syncing existing proxies. Authentication rejection stops reconnection for that session—fix the compiled configuration and start a new session.

> **Important:** An RPC can commit a write before a timeout or disconnect is observed. Fetch a fresh snapshot before deciding whether to retry a business action; there is no automatic rollback or exactly-once replay.

## Lua values and errors

Parameters and snapshot data accept ordinary Lua `nil`, booleans, finite numbers, byte strings and acyclic tables. Mixed and sparse boolean/number/string keys and signed 64-bit integers are preserved; functions, threads, userdata, class/native objects, metatables, cycles and table keys are rejected. See [Lua data and wire encoding](<HTTP Protocol.md#lua-data-and-wire-encoding>) for exact value and size rules (these values do not use the `RuntimeData` JSON conversion contract).

Network results use **string codes**, including `InvalidArgument`, `AuthenticationFailed`, `Timeout`, `ConnectionFailed`, `InvalidResponse`, `QueueFull`, `StorageFailure`, `ServerFailure` and `RateLimited`, plus any valid custom server codes. Branch on `code` and log `message` for diagnostics; when `retryAfterSeconds` is present, wait at least that many whole seconds before retrying.

## Supplied Storage module

Require `Global.Net.Storage` to register the built-in client module and its `Global.Net.StorageProxy` class (the server entry must register `Modules.Storage` under `"Storage"`). Fetch snapshots with `Net.GetProxy("Storage", params, callback)`:

| `params.query` | Proxy getters |
| --- | --- |
| `"account_exists"` | `AccountExists()` |
| `"accounts"` | `GetAccounts()` |
| `"field"` | `IsFound()`, `GetFieldValue()` |
| `"fields"` | `GetEntries()`, `GetNextCursor()` |

Each getter reads only that proxy's query result; unrelated getters may return `nil`. Entries expose `key` and `value`, and a `nil` next cursor ends pagination (pass another cursor in a new `GetProxy` request). Query parameters, account sampling, field ordering, missing values and storage limits are defined in [Storage services](<HTTP Protocol.md#storage-services>).

Write with `Storage.CallRpcFunction("WriteField", account, category, field, value)` (`nil` deletes the field). Success emits `FieldWritten(account, category, field)` to the caller. This supplied RPC alone uses the [configured write interval](<HTTP Protocol.md#write-rate-limits>) (default 180 seconds); custom RPCs do not inherit it, and there is no write-token API.

## Network quality

When integration is enabled, the independent `GlobalCore.Server.CheckConnectionAsync(callback)` runs a 3-second HTTP health probe. Custom entry points must initialise `GlobalFunctions.NodeGraph.initLatent()` and pump operation updates. Its callback receives `GlobalCore.ServerConnectionResult`; narrow the generated nullable native-object declaration with `assert(result)`:

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

`Normal` indicates a valid configured-service health response within 300 ms, `Weak` indicates over 300 ms within 3 seconds and `Disconnected` indicates no valid response or a failed request. The result also exposes `ok`, `code` (`GlobalCore.ServerErrorCode`), `message` and `roundTripMs` when a valid response arrives. It returns an `Engine.AsyncOperation` with standard callback/result identity and cancellation semantics.

The health endpoint does not verify the project key, so a healthy result confirms neither RPC authentication nor general Internet connectivity. In native clients, system DNS resolution can stay blocked after cancellation and delay session shutdown; cancellation does not guarantee a hard DNS or shutdown deadline.
