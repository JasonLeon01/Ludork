# Native Client API

Configure the URL and key through [Deployment and Editor Settings](<Deployment and Editor Settings.md>). Network transport, JSON conversion, write-token ownership and completion scheduling run in C++; Lua exposes the typed `GlobalCore.Server` methods without URL or key parameters. The transport uses SFML networking over HTTP.

## Methods and results

Every method requires a completion callback and returns an `Engine.AsyncOperation`. Call from the game logic thread in a running game whose latent-operation updates are initialised. The default Entry calls `GlobalFunctions.NodeGraph.initLatent()`; custom entry points must initialise it too. See [Lua Runtime and Modules](<../Lua and Blueprint Scripting/Lua Runtime and Modules.md#asynchronous-operation-updates>) for the operation lifecycle. Starting an operation in a bare script without the game update loop does not drive its callback.

| `GlobalCore.Server` method | Callback argument |
|---|---|
| `CheckAccountExistsAsync(accountId, onCompleted)` | `GlobalCore.ServerAccountResult` |
| `ListAccountsAsync(sampleCount, categoryFilter, onCompleted)` | `GlobalCore.ServerAccountListResult` |
| `ReadFieldAsync(accountId, category, field, onCompleted)` | `GlobalCore.ServerReadResult` |
| `ListFieldsAsync(accountId, category, beforeKey, limit, onCompleted)` | `GlobalCore.ServerFieldListResult` |
| `AcquireWriteTokenAsync(accountId, onCompleted)` | `GlobalCore.ServerTokenResult` |
| `WriteFieldAsync(token, category, field, value, onCompleted)` | `GlobalCore.ServerResult` |
| `CheckConnectionAsync(onCompleted)` | `GlobalCore.ServerConnectionResult` |

All results expose read-only `ok`, `code`, `message` and `retryAfterSeconds: integer|nil` properties. `ok` is true exactly when `code == GlobalCore.ServerErrorCode.None`. Use the enum for decisions and `message` for diagnostics. `retryAfterSeconds` is present only for a rate-limited write. Check `ok` before using query data; an empty result on failure is not a successful empty query.

Optional list parameters still occupy their positional arguments: pass `nil` for defaults, keeping the required callback last.

| Result | Additional properties |
|---|---|
| `ServerAccountResult` | Extends `ServerResult` with read-only `exists: boolean`; meaningful only when `ok == true`. |
| `ServerAccountListResult` | `accounts: string[]`; iterate the returned account IDs with `ipairs`. |
| `ServerReadResult` | `found: boolean`, `value: Engine.RuntimeDataValue`; a missing field has `found == false`, while a stored JSON null has `found == true`. |
| `ServerFieldListResult` | `entries: GlobalCore.ServerFieldEntry[]`, `nextCursor: string\|nil`; a nil cursor ends pagination. |
| `ServerTokenResult` | `token: GlobalCore.ServerWriteToken\|nil`; present on successful acquisition. |
| `ServerConnectionResult` | `state: GlobalCore.ServerNetworkState`, `roundTripMs: number\|nil`; timing is present only for a valid health response. |

The native declarations are in `<Server.hpp>` and `<Server/...hpp>`. C++ callbacks receive the corresponding `std::shared_ptr<Result>`; generated Lua declarations therefore include `nil`, so narrow it with `assert(result)` in Lua. Results and tokens are returned native objects, not tables to construct yourself. `ServerFieldEntry` is a returned value record with read-only `key: string` and `value: Engine.RuntimeDataValue`; the entries array contains non-nil records. `ServerWriteToken` is opaque: Lua cannot inspect or supply its raw token or bound account.

Completion first stores the result on the operation, then invokes the callback on the game logic thread. `operation:getResult()` after completion returns the **same result object** delivered to that callback. A request error completes normally with `ok == false`; it is not cancellation. Explicit cancellation suppresses the completion callback and asks the transport to stop. It cannot undo a write already accepted by the server. Closing a game session also cancels its pending work without calling Lua from a worker thread.

## Check account existence

Use `CheckAccountExistsAsync` to query an account:

```lua
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")

GlobalCore.Server.CheckAccountExistsAsync("player-1", function(result)
    assert(result)
    if not result.ok then
        Logging.warning("Account query failed: %s", result.message)
    elseif result.exists then
        Logging.info("Account exists")
    else
        Logging.info("Account is absent")
    end
end)
```

An account exists when its data directory exists, even if empty. The check does not create or reserve it, and provides no atomic collision guarantee. Generate UUID account identifiers in your application. Storage failures remain errors rather than `exists == false`; see [account-existence semantics](<HTTP Protocol.md#check-account-existence>).

## List accounts and fields

### List or sample accounts

`ListAccountsAsync(nil, nil, onCompleted)` and `sampleCount == 0` return all eligible account IDs in ascending UTF-8 byte order. Positive counts sample distinct accounts in random order, up to that count. `categoryFilter` applies before sampling and tests file presence rather than JSON validity. See [List accounts](<HTTP Protocol.md#list-accounts>) for accepted ranges, storage errors and the 2 MiB response limit.

This gameplay example samples up to ten accounts that have a `history` category:

```lua
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")

GlobalCore.Server.ListAccountsAsync(10, "history", function(result)
    assert(result)
    if not result.ok then
        Logging.warning("Account list failed: %s", result.message)
        return
    end
    for _, accountId in ipairs(result.accounts) do
        Logging.info("Account: %s", accountId)
    end
end)
```

There is no account-list cursor. An oversized result fails with `InvalidArgument`; request a smaller positive `sampleCount`.

### Page through a category's fields

Fields are ordered by **descending UTF-8 bytes**, independently of locale or numeric interpretation. `beforeKey == nil` starts the first page; a supplied key is an exclusive upper bound, so only smaller keys are returned, even if the bound itself is not stored. `limit == nil` means 50; explicit limits must be integers from 1 to 100. `beforeKey` follows the field-name rules below.

When more fields remain, `nextCursor` is the last returned key. Pass it unchanged as the next `beforeKey`; stop when it is nil. A missing account/category or an exhausted range succeeds with empty `entries` and a nil cursor. A corrupt category fails with `StorageFailure`.

Read successive pages with the returned cursor:

```lua
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")

local function readPage(beforeKey)
    GlobalCore.Server.ListFieldsAsync("player-1", "history", beforeKey, nil, function(result)
        assert(result)
        if not result.ok then
            Logging.warning("Field list failed: %s", result.message)
            return
        end
        for _, entry in ipairs(result.entries) do
            Logging.info("%s = %s", entry.key, tostring(entry.value))
        end
        if result.nextCursor ~= nil then
            readPage(result.nextCursor)
        end
    end)
end

readPage(nil)
```

Pages do not form a snapshot across writes. Refresh from the first page to include newly inserted keys above the cursor. For timestamp-key design and response-size limits, see [List category fields](<HTTP Protocol.md#list-category-fields>).

## Read and write from Lua

The following example writes one field, then reads it back:

```lua
local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")

local Server = GlobalCore.Server

Server.AcquireWriteTokenAsync("player-1", function(result)
    assert(result)
    if not result.ok then
        Logging.warning("Token request failed: %s", result.message)
        return
    end
    local token = assert(result.token)
    Server.WriteFieldAsync(token, "profile", "score", 100, function(written)
        assert(written)
        if not written.ok then
            if written.code == GlobalCore.ServerErrorCode.RateLimited then
                Logging.warning("Wait at least %d seconds, then acquire a new write token.",
                    assert(written.retryAfterSeconds))
            else
                Logging.warning("Write failed: %s", written.message)
            end
            return
        end
        Server.ReadFieldAsync("player-1", "profile", "score", function(read)
            assert(read)
            if not read.ok then
                Logging.warning("Read failed: %s", read.message)
            elseif read.found then
                Logging.info("Score: %s", tostring(read.value))
            else
                Logging.info("Score is absent")
            end
        end)
    end)
end)
```

Pass account, category and field names without URL-encoding; the client encodes each path segment. Names must satisfy the [HTTP identifier rules](<HTTP Protocol.md>), including the 1–64 UTF-8 byte limit.

Values follow the [RuntimeData conversion rules](<../Native C++ Development/Runtime Value Boundaries.md#lua-value-conversion>). A top-level null value stores null rather than deleting the field. The backend and viewer preserve the digits of JSON numbers; they cannot recover precision already lost by a caller's numeric conversion.

A successful write creates the account/category as needed and replaces one top-level dictionary field. The [write contract](<HTTP Protocol.md#acquire-and-use-a-token>) defines the 1 MiB request/file limit and transaction boundaries; there is no automatic save synchronisation.

## Tokens and timeouts

A write token is bound to the project, account and current client session, lasts **at most 60 seconds**, and permits **one write**. The client consumes a valid token before sending. Failed or timed-out writes are not retried automatically: read the field before deciding whether to acquire another token and write again. Backend token handling and restart invalidation follow the [HTTP write contract](<HTTP Protocol.md#acquire-and-use-a-token>).

The server also applies a configurable write interval per project and client IP; see [HTTP write rate limits](<HTTP Protocol.md#write-rate-limits>) for the full rules.

A denied write completes with `ServerErrorCode.RateLimited` and `retryAfterSeconds`, rounded up to whole seconds. **The valid token is already consumed.** Wait at least that delay, then acquire a fresh token when ready to write; the default interval exceeds the token lifetime.

Ordinary operations have a ten-second deadline; the connection probe has a three-second deadline. Work executes away from the game logic thread, while completion is delivered on its next operation update. System DNS resolution may remain blocked after cancellation; stopping the game can wait for that resolver to return. No DNS cancellation or hard shutdown deadline is promised.

## Connection quality

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

| `GlobalCore.ServerNetworkState` | Meaning |
|---|---|
| `Normal` | Valid server health response in at most 300 ms. |
| `Weak` | Valid response above 300 ms, within the three-second deadline. |
| `Disconnected` | No valid response within that deadline, or a disabled/failed request. |

This probes the configured server, not general Internet access. The health endpoint does not check the project key, so a Normal result does not establish that subsequent reads or writes will authenticate.

## Error codes

`GlobalCore.ServerErrorCode` contains `None`, `Disabled`, `InvalidArgument`, `AuthenticationFailed`, `InvalidWriteToken`, `Timeout`, `ConnectionFailed`, `InvalidResponse`, `StorageFailure`, `ServerFailure` and `RateLimited`.

| Code | Meaning |
|---|---|
| `None` | The operation succeeded. |
| `Disabled` | This build has no enabled Ludork Server transport. |
| `InvalidArgument` | Invalid identifier, URL, JSON value or size. |
| `AuthenticationFailed` | The project key or management authentication was rejected. |
| `InvalidWriteToken` | Missing, expired, wrong-session/account/project or already consumed token. |
| `Timeout` | The operation deadline elapsed. |
| `ConnectionFailed` | The transport could not establish or maintain communication. |
| `InvalidResponse` | The peer returned an invalid HTTP/JSON or operation response. |
| `StorageFailure` | The backend could not read or write stored data. |
| `ServerFailure` | Another backend failure occurred. |
| `RateLimited` | The project's write interval for this client IP has not elapsed; use `retryAfterSeconds` and a fresh token after waiting. |

The disabled build retains the same API and asynchronous result shape. A missing callback is a programming error and is rejected at the call boundary; request outcomes use the typed result above.

For direct HTTP clients and management endpoints, see [HTTP Protocol](<HTTP Protocol.md>).
