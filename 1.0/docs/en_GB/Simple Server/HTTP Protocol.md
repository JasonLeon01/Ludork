# Server Lua and Protocol

Node.js owns connections, project authentication, storage and process supervision. Project Lua 5.4.5 modules implement explicit snapshots, void RPCs and named events. Each project runs in a separate Wasmoon process; the game client's Standard extensions and native modules are unavailable on the server. Client proxy usage belongs to [Native Client API](<Native Client API.md>), and project entry paths belong to [Deployment and Editor Settings](<Deployment and Editor Settings.md#project-lua-entry>).

## Server Lua modules

The project entry loads framework modules from `lua/` and project modules from its own private directory. Register modules explicitly:

```lua
local Net = require("Framework.Net")
local Storage = require("Modules.Storage")
local Profile = require("Profile")

Net.RegisterModule("Storage", Storage)
Net.RegisterModule("Profile", Profile)
```

A project `Profile.lua` can define both reads and writes:

```lua
local Profile = {Rpc = {}}

function Profile.Snapshot(context, params)
    if type(params) ~= "table" or type(params.account) ~= "string" then
        error({code = "InvalidArgument", message = "An account is required."})
    end
    local field = context.Storage:ReadField(params.account, "profile", "name")
    return {name = field.value or "New player"}
end

function Profile.Rpc.SetName(context, account, name)
    if type(name) ~= "string" or #name > 80 then
        error({code = "InvalidArgument", message = "Name must be at most 80 bytes."})
    end
    context.Storage:WriteField(account, "profile", "name", name)
    context:Notify("NameChanged", account)
end

return Profile
```

Module, RPC and event names are case-sensitive: 1–128 ASCII letters, digits, dots, underscores or hyphens. Duplicate registration fails initialization. Only functions placed in the module's `Rpc` table are remotely callable. Module definitions may omit `Snapshot` or `Rpc` when that operation is not provided.

`Snapshot(context, params)` receives one pure Lua value and must return an ordinary table. Every fetch calls it again. Its storage access is read-only, and it cannot send events. The server does not retain the client's proxy or replicate changes automatically.

`Rpc.FunctionName(context, ...)` receives positional pure Lua arguments, including nil gaps and trailing nil values. It is void: return values are discarded. Calls from one connection retain send order, and each project processes its handlers serially. Host storage calls suspend the current coroutine until Node finishes, allowing sequential Lua code. Do not retain a context or use its services after the handler returns.

| Context member | Meaning |
| --- | --- |
| `ProjectName`, `ModuleName` | Current project and registered module. |
| `SessionId` | Opaque ID for the current online connection. |
| `PeerAddress` | Actual TCP peer address; forwarded headers are ignored. |
| `Storage` | Storage services confined to this project. |
| `Notify(event, ...)` | Queue an event for the caller. |
| `Send(sessionId, event, ...)` | Queue an event for one online connection in this project. |
| `Broadcast(event, ...)` | Queue an event for every online connection in this project. |
| `AdmitWrite(account, category, field)` | Apply the configured interval inside the supplied `Storage.WriteField` RPC only. |

Call context methods with `:`. Events carry the current module name. They are flushed after the handler succeeds; a failed handler discards its queued events. This does not roll back storage writes already completed by that handler. Disconnected recipients receive no backlog. Events never refresh client proxy data.

An ordinary Lua error fails the request with `ServerFailure`. A handler can raise a table with string `code` and `message`, and an optional positive integer `retryAfterSeconds`. Codes use an ASCII letter followed by up to 63 ASCII letters or digits; invalid codes become `ServerFailure`. Host errors retain their stable codes. Validation and account authorization remain project logic: the shared project key does not authenticate individual account IDs.

A project has a 64 MiB Lua allocator limit, a nine-second Lua execution interrupt budget and a ten-second overall invocation deadline including queue time. A crash, active invocation timeout or memory failure invalidates that project's connections and restarts its Lua state. Its previous pending calls fail and are not replayed; new requests during recovery wait in the bounded queue for the replacement state, within their original deadlines. Other projects, health checks and management remain available. An entry that cannot load fails that project's requests until corrected and restarted. Server logic cannot use `io`, `os`, `debug`, `load`, `loadfile`, `dofile` or native library loading; it accesses persistence through the context.

## Storage services

Project, account and category filesystem names use lowercase hexadecimal UTF-8 encoding, preserving case and avoiding reserved filenames and path traversal. Account, category and field names accept 1–64 UTF-8 bytes without control characters, excluding the exact names `.` and `..`. Account/category/field names may contain slash characters; project names may not. Account IDs are supplied by game logic. The server does not generate UUIDs, reserve IDs or authenticate accounts.

Each category is a data-only `.ldata` file with the envelope `{version:1,data:WireValue}`. Its root is a table keyed by field name and uses the [tagged encoding](<#lua-data-and-wire-encoding>) below. The file is never executed as Lua. Existing `.json` category files are not read or converted; there is no compatibility reader or automatic migration.

| `context.Storage` method | Result |
| --- | --- |
| `AccountExists(account)` | Boolean: whether the account directory exists, including an empty directory. |
| `ListAccounts(sampleCount, categoryFilter)` | Array of eligible account names. |
| `ReadField(account, category, field)` | `{found = boolean, value = value}`; missing fields have `found = false` and nil value. |
| `ListFields(account, category, beforeKey, limit)` | `{entries = {{key = key, value = value}, ...}, nextCursor = cursor}`. |
| `WriteField(account, category, field, value)` | nil; replace the field, or remove it when value is nil. |

Use `:` for these methods. Reads never create accounts. Missing accounts/categories produce empty lists or missing-field results; filesystem inspection and corrupt data failures produce `StorageFailure`, not absence.

`sampleCount` accepts nonnegative signed 64-bit integers. Nil or zero returns every matching account in ascending UTF-8 byte order. A positive count samples distinct accounts in random order, capped at the eligible count. `categoryFilter` checks for a regular `.ldata` file before sampling, without validating its contents. There is no account-list cursor; reduce the sample size if the complete response exceeds the message limit.

Fields are ordered by descending UTF-8 bytes, independently of locale or numeric interpretation. Nil `beforeKey` starts the first page; a supplied key is an exclusive upper bound, whether stored or not. `limit` defaults to 50 and accepts integers from 1 through 100. If more matching fields remain, `nextCursor` is the last returned key; otherwise it is nil. Empty ranges return empty entries and nil cursor. Pagination reads current files, without a snapshot across requests. Refresh from the first page to include new keys above a cursor; applications can use fixed-width timestamps when byte order should represent time.

Writes create the account/category as needed. Nil removes the field but leaves the category file and account directory. Same-file updates are serialized and replace a temporary file atomically; distinct files can update independently. Each complete category file is limited to **1 MiB**, including encoding and its final newline. There are no cross-file transactions, automatic backups or rollback of earlier writes when a later statement fails. A timeout or lost response can occur after a write commits.

The supplied `Modules.Storage` module exposes these snapshot queries:

| `params.query` | Parameters | Snapshot |
| --- | --- | --- |
| `"account_exists"` | `account` | `{exists = boolean}` |
| `"accounts"` | Optional `sampleCount`, `categoryFilter` | `{accounts = {...}}` |
| `"field"` | `account`, `category`, `field` | `{found = boolean, value = value}` |
| `"fields"` | `account`, `category`; optional `beforeKey`, `limit` | `{entries = {...}, nextCursor = cursor}` |

Its `WriteField(account, category, field, value)` RPC applies the configured interval, writes the field and emits `FieldWritten(account, category, field)` to the caller. Custom modules call the same storage services directly and implement their own business rules.

### Write rate limits

The configured `ipWriteIntervalSeconds`, default **180 seconds**, applies only to the supplied `Storage.WriteField` RPC. See [configuration](<Deployment and Editor Settings.md#configure-the-write-interval>). It is shared across that project's accounts/categories for the same TCP client IP. IPv4-mapped IPv6 addresses are normalized to IPv4; `X-Forwarded-For` is ignored. Clients behind one NAT or reverse proxy can share an allowance. Other projects have separate allowances.

Admission starts the interval before storage, and a later storage failure still consumes it. Rejection does not extend the deadline. `RateLimited` carries positive `retryAfterSeconds`, rounded up to whole seconds. Deadlines use a monotonic clock and reset on service restart. Reads, health, management and custom RPCs do not consume this allowance. Custom RPCs are not automatically rate-limited, and cannot invoke `AdmitWrite` to consume this built-in interval.

## Lua data and wire encoding

Parameters, snapshots, events and stored values accept nil, booleans, finite numbers, byte strings and ordinary acyclic tables. Integers preserve the complete signed 64-bit range. Tables may have mixed or sparse boolean, numeric and string keys. Nil is a valid positional argument; in a table it means no entry. Empty tables remain tables.

Functions, threads, userdata, metatables, cyclic references and table keys are rejected. Repeated references to an acyclic table are encoded as independent copies: object identity is not preserved. These rules also exclude Ludork class instances and native containers from network data.

Frames carry UTF-8 JSON, with each Lua value represented by one tagged array:

```text
["nil"]
["bool", true]
["int", "9223372036854775807"]
["float", "1.2345678901234567"]
["string", "Ynl0ZXM="]
["table", [[WireKey, WireValue], ...]]
```

Integers use exact decimal text; floats use finite double-precision decimal text that round-trips. Strings encode arbitrary bytes using canonical base64. `WireKey` accepts only the boolean, integer, float or string forms. Duplicate equivalent keys, such as integer `1` and float `1.0`, are invalid. Table entries with nil values are omitted.

A message is limited to **2 MiB**; data has at most **64 nesting levels** and **65,536 value nodes**. RPC/event argument arrays preserve every nil position. Limits count the encoded message, so base64 and structural overhead matter.

## WebSocket messages

Connect to `ws://host:7777/<URL-encoded-project>/api/v1/net` with `X-Ludork-Key` on the HTTP upgrade. The server answers `{"type":"ready","version":1}` before application messages. Credentials do not occur in URLs or frame bodies. Binary frames and message compression are not used.

```text
snapshot: {type:"snapshot", id:"1", module:"Profile", params:WireValue}
RPC:      {type:"rpc", id:"2", module:"Profile", name:"SetName", args:[WireValue,...]}
success:  {type:"result", id:"1", ok:true, data:WireValue}
RPC ack:  {type:"result", id:"2", ok:true}
failure:  {type:"result", id:"2", ok:false, code:"InvalidArgument", message:"..."}
event:    {type:"event", module:"Profile", event:"NameChanged", args:[WireValue,...]}
```

Request IDs use 1–128 ASCII letters, digits, dots, underscores or hyphens and must be unique among pending requests on one connection. A duplicate pending ID closes the connection. A rate failure may include `retryAfterSeconds`. RPC acknowledgments confirm execution without a business return value. Server events follow the successful acknowledgment.

Incoming work is capped at 256 calls and 8 MiB per connection and per project. Outgoing buffering is capped at 8 MiB per connection; slow connections are closed. One handler can queue at most 256 events and 8 MiB of encoded event messages. Ping/pong checks remove dead connections. Client timeout, cancellation and recovery rules are in [Call RPCs and receive events](<Native Client API.md#call-rpcs-and-receive-events>).

## HTTP health and read-only management

The HTTP API base remains `http://host:7777/<project>/api/v1`. `GET /health` requires no key and returns `{"service":"LudorkServer","project":"MyGame"}`. Data queries and writes use the WebSocket module APIs; there are no account/field REST or write-token endpoints.

Management paths are under `/admin`. Vite on port 3333 proxies them to port 7777; the backend enforces authentication and does not enable CORS.

| Method and path below `/admin` | Requirement | Success body |
| --- | --- | --- |
| `POST /login` | JSON `{"key":"...","password":"..."}` and `X-Ludork-Admin: 1` | `{"project":"MyGame"}`, plus session cookie. |
| `GET /session` | Session cookie | `{"project":"MyGame"}` |
| `POST /logout` | Session cookie and `X-Ludork-Admin: 1` | `{}`, clears the cookie. |
| `GET /accounts` | Session cookie | `{"accounts":["player-1"]}` |
| `GET /accounts/:account/categories` | Session cookie | `{"categories":["profile"]}` |
| `GET /accounts/:account/categories/:category` | Session cookie | `{value:WireValue}` for the category table. |

Encode each path parameter as one URL segment. Sessions are project-bound, expire after eight hours and are invalidated by logout or backend restart. Cookies are HttpOnly, SameSite=Strict and scoped to the project's management API. Responses use `Cache-Control: no-store`; the frontend does not keep credentials in browser storage. There is no management write/delete API. The viewer displays exact integers, typed keys and escaped byte strings.

Non-2xx HTTP responses contain `code`, `message` and optional `retryAfterSeconds`. Common statuses are 400 `InvalidArgument`, 401/403 `AuthenticationFailed`, 404 `InvalidArgument` for missing resources, 429 `RateLimited`, and 500 `StorageFailure` or `ServerFailure`. A rate-limited HTTP response also sets `Retry-After`. WebSocket invocation failures use result frames, not HTTP status codes.

HTTP and WebSocket traffic are unencrypted, and a shared key grants access to the project's registered modules. Review the [deployment trust boundary](<Deployment and Editor Settings.md#management-and-operational-limits>) before exposing the service.
