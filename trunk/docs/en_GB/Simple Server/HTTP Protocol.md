# Server Lua and Protocol

Node.js owns connections, project authentication, storage and process supervision, while project Lua 5.4.5 modules implement explicit snapshots, void RPCs and named events. Each project runs in an isolated Wasmoon process without client Standard extensions or native modules. See [Native Client API](<Native Client API.md>) for client proxy usage and [Deployment and Editor Settings](<Deployment and Editor Settings.md#project-lua-entry>) for entry paths.

## Server Lua modules

The project entry loads framework modules from `lua/` and project modules from its private directory, registering modules explicitly:

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

- **Naming and registration:** Module, RPC and event names are case-sensitive and consist of 1–128 ASCII letters, digits, dots, underscores or hyphens. Duplicate registration fails initialisation. Only functions in the module's `Rpc` table are remotely callable; modules may omit `Snapshot` or `Rpc` if unused.
- **Snapshots (`Snapshot(context, params)`):** Receives one pure Lua value and must return an ordinary table. Every fetch invokes it again with read-only storage access and no event emission. The server neither caches client proxies nor replicates changes automatically.
- **RPCs (`Rpc.FunctionName(context, ...)`):** Receives positional pure Lua arguments, including `nil` gaps and trailing `nil` values, and discards return values. Calls from one connection execute in send order, and each project executes handlers serially. Host storage calls suspend the current coroutine until Node.js finishes, enabling sequential Lua code. Do not retain `context` or call its services after a handler returns.

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

Call context methods with `:`. Events carry the current module name and flush after the handler succeeds; a failed handler discards its queued events without rolling back completed storage writes. Disconnected recipients receive no backlog, and events never refresh client proxy data.

An ordinary Lua error fails the request with `ServerFailure`. Handlers may instead raise a table with string `code`, string `message` and optional positive integer `retryAfterSeconds`; codes must begin with an ASCII letter followed by up to 63 ASCII letters or digits (invalid codes map to `ServerFailure`), while host errors keep their stable codes.

Validation and account authorisation are project responsibilities because the shared project key does not authenticate individual account IDs.

- **Execution limits:** Each project has a 64 MiB Lua allocator limit, a 9-second Lua execution interrupt budget and a 10-second overall invocation deadline including queue time.
- **Recovery and isolation:** A crash, active invocation timeout or memory failure invalidates that project's connections and restarts its Lua state. Previous pending calls fail without replay; new requests during recovery wait in the bounded queue within their original deadlines, while sibling projects, health checks and management stay available. An entry that fails to load fails that project's requests until corrected and restarted.
- **Sandbox restrictions:** Server logic cannot use `io`, `os`, `debug`, `load`, `loadfile`, `dofile` or native library loading; all persistence goes through `context.Storage`.

## Storage services

Project, account and category filesystem names use lowercase hexadecimal UTF-8 encoding to preserve case and avoid reserved filenames and path traversal. Account IDs are supplied by game logic—the server does not generate UUIDs, reserve IDs or authenticate accounts.

Account, category and field names accept 1–64 UTF-8 bytes without control characters, excluding the exact names `.` and `..`; they may contain slashes, whereas project names may not.

Each category is a data-only `.ldata` file with the envelope `{version:1,data:WireValue}` whose root is a table keyed by field name using the [tagged encoding](<#lua-data-and-wire-encoding>) below. The file is never executed as Lua. Existing `.json` category files are not read or converted; there is no compatibility reader or automatic migration.

| `context.Storage` method | Result |
| --- | --- |
| `AccountExists(account)` | Boolean: whether the account directory exists, including an empty directory. |
| `ListAccounts(sampleCount, categoryFilter)` | Array of eligible account names. |
| `ReadField(account, category, field)` | `{found = boolean, value = value}`; missing fields return `found = false` and `value = nil`. |
| `ListFields(account, category, beforeKey, limit)` | `{entries = {{key = key, value = value}, ...}, nextCursor = cursor}`. |
| `WriteField(account, category, field, value)` | `nil`; replaces the field, or deletes it when `value` is `nil`. |

- **Calling and missing data:** Call `context.Storage` methods with `:`. Reads never create accounts; missing accounts or categories yield empty lists or missing-field results, whereas filesystem inspection and corrupt-data errors raise `StorageFailure`.
- **Account listing (`ListAccounts`):** `sampleCount` accepts non-negative signed 64-bit integers (`nil` or `0` returns all matching accounts in ascending UTF-8 byte order; a positive count samples distinct accounts in random order, capped at the eligible count). `categoryFilter` checks for a regular `.ldata` file without validating its contents. There is no cursor—reduce `sampleCount` if the response exceeds the message limit.
- **Field ordering and cursors (`ListFields`):** Fields sort in descending UTF-8 byte order regardless of locale or numeric interpretation. `beforeKey = nil` starts the first page; a supplied key is an exclusive upper bound whether stored or not. `limit` defaults to `50` (range `1–100`). `nextCursor` is the last returned key when more fields follow, or `nil` otherwise (empty ranges return empty `entries` and `nextCursor = nil`).
- **Live pagination (`ListFields`):** Pagination reads live files without a cross-request snapshot. Refresh from the first page to see newer keys above a cursor, and use fixed-width timestamps when byte order should represent time.
- **Writes and atomicity (`WriteField`):** Writes create the account and category as needed; writing `nil` removes the field while keeping the category file and account directory. Same-file updates are serialised and committed atomically via a temporary file, while distinct files update independently.
- **File size and transaction limits:** Each complete category file is limited to **1 MiB**, including encoding and its trailing newline. There are no cross-file transactions, automatic backups or rollbacks of earlier writes if a later statement fails, and a timeout or lost response can occur after a write commits.

The supplied `Modules.Storage` module exposes these snapshot queries:

| `params.query` | Parameters | Snapshot |
| --- | --- | --- |
| `"account_exists"` | `account` | `{exists = boolean}` |
| `"accounts"` | Optional `sampleCount`, `categoryFilter` | `{accounts = {...}}` |
| `"field"` | `account`, `category`, `field` | `{found = boolean, value = value}` |
| `"fields"` | `account`, `category`; optional `beforeKey`, `limit` | `{entries = {...}, nextCursor = cursor}` |

Its `WriteField(account, category, field, value)` RPC applies the configured interval, writes the field and emits `FieldWritten(account, category, field)` to the caller. Custom modules call `context.Storage` directly and implement their own business rules.

### Write rate limits

The configured `ipWriteIntervalSeconds` (default **180 seconds**; see [configuration](<Deployment and Editor Settings.md#configure-the-write-interval>)) applies only to the supplied `Storage.WriteField` RPC and is shared across a project's accounts and categories for the same TCP client IP. IPv4-mapped IPv6 addresses normalise to IPv4, and `X-Forwarded-For` is ignored, so clients behind one NAT or reverse proxy share one allowance while separate projects maintain separate allowances.

Admission starts the interval before storage runs, so a subsequent storage failure consumes it; rejections do not extend the deadline. `RateLimited` includes a positive `retryAfterSeconds` rounded up to whole seconds. Deadlines use a monotonic clock and reset on service restart. Reads, health checks, management and custom RPCs do not consume this allowance, and custom RPCs cannot invoke `AdmitWrite` to use the built-in interval.

## Lua data and wire encoding

Parameters, snapshots, events and stored values accept `nil`, booleans, finite numbers, byte strings and ordinary acyclic tables. Integers preserve the full signed 64-bit range, tables may have mixed or sparse boolean, numeric and string keys, `nil` is valid as a positional argument (representing absence in a table) and empty tables encode as empty tables.

Functions, threads, userdata, metatables, cyclic references and table keys are rejected. Repeated references to an acyclic table encode as independent copies without preserving object identity. These rules exclude Ludork class instances and native containers from network data.

Frames carry UTF-8 JSON with each Lua value encoded as one tagged array:

```text
["nil"]
["bool", true]
["int", "9223372036854775807"]
["float", "1.2345678901234567"]
["string", "Ynl0ZXM="]
["table", [[WireKey, WireValue], ...]]
```

Integers use exact decimal text, floats use round-tripping finite double-precision decimal text and strings encode arbitrary bytes as canonical Base64. `WireKey` accepts only boolean, integer, float or string forms; duplicate equivalent keys (such as integer `1` and float `1.0`) are invalid, and table entries with `nil` values are omitted.

Messages are limited to **2 MiB**, with at most **64 nesting levels** and **65,536 value nodes**. RPC and event argument arrays preserve every `nil` position. Limits apply to the encoded frame, including Base64 and structural overhead.

## WebSocket messages

Connect to `ws://host:6666/<URL-encoded-project>/api/v1/net`, or `ws://host:6666/api/v1/net` for a [bound project domain](<Deployment and Editor Settings.md#bind-a-project-domain>). Browser HTTPS endpoints use WSS through their TLS proxy. Credentials never appear in URLs, and binary frames and message compression are not used.

Send `{"type":"auth","key":"<project-key>"}` as the first message; the server replies with `{"type":"ready","version":1}` before any application frames. Authentication has a 10-second deadline—failure closes the connection with code `4401`, and the client stops reconnecting for that session.

```text
snapshot: {type:"snapshot", id:"1", module:"Profile", params:WireValue}
RPC:      {type:"rpc", id:"2", module:"Profile", name:"SetName", args:[WireValue,...]}
success:  {type:"result", id:"1", ok:true, data:WireValue}
RPC ack:  {type:"result", id:"2", ok:true}
failure:  {type:"result", id:"2", ok:false, code:"InvalidArgument", message:"..."}
event:    {type:"event", module:"Profile", event:"NameChanged", args:[WireValue,...]}
```

Request IDs consist of 1–128 ASCII letters, digits, dots, underscores or hyphens and must be unique among pending requests on a connection (a duplicate pending ID closes the connection). Rate-limit failures may include `retryAfterSeconds`. RPC acknowledgements confirm execution without a business return value, and server events follow the successful acknowledgement.

- **Queue and buffer caps:** Incoming work is capped at 256 calls and 8 MiB per connection and per project; outgoing buffering is capped at 8 MiB per connection (slow connections are closed).
- **Event caps and keepalive:** One handler can queue at most 256 events and 8 MiB of encoded event messages. Ping/pong frames detect and remove dead connections. See [Call RPCs and receive events](<Native Client API.md#call-rpcs-and-receive-events>) for client timeout, cancellation and recovery rules.

## HTTP health and read-only management

The HTTP API base is `http://host:6666/<project>/api/v1` (or `http://host:6666/api/v1` for a bound project domain). `GET /health` requires no key and returns `{"service":"LudorkServer","project":"MyGame"}`: a URL naming a project requires that exact project, whereas a project-less URL accepts any non-empty project name.

Data queries and writes use WebSocket module APIs; there are no account/field REST or write-token endpoints.

`GET /health` permits cross-origin browser requests via `Access-Control-Allow-Origin: *`, and browser clients omit credentials. This permission applies only to the public health route.

Management routes live under `/admin`. The production Node.js service on port `3333` serves the built website and proxies same-origin API requests to port `6666`; the backend enforces authentication and does not enable CORS for management.

| Method and path below `/admin` | Requirement | Success body |
| --- | --- | --- |
| `POST /login` | JSON `{"key":"...","password":"..."}` and `X-Ludork-Admin: 1` | `{"project":"MyGame"}`, plus session cookie. |
| `GET /session` | Session cookie | `{"project":"MyGame"}` |
| `POST /logout` | Session cookie and `X-Ludork-Admin: 1` | `{}`, clears the cookie. |
| `GET /accounts` | Session cookie | `{"accounts":["player-1"]}` |
| `GET /accounts/:account/categories` | Session cookie | `{"categories":["profile"]}` |
| `GET /accounts/:account/categories/:category` | Session cookie | `{value:WireValue}` for the category table. |

Encode each path parameter as one URL segment. Sessions are project-bound, expire after eight hours and are invalidated by logout or backend restart; cookies are `HttpOnly`, `SameSite=Strict` and scoped to the project's management API.

Responses set `Cache-Control: no-store`, and the frontend never stores credentials in browser storage. There is no management write or delete API; the viewer renders exact integers, typed keys and escaped byte strings.

Non-2xx HTTP responses include `code`, `message` and optional `retryAfterSeconds`. Common statuses are `400` (`InvalidArgument`), `401`/`403` (`AuthenticationFailed`), `404` (`InvalidArgument` for missing resources), `429` (`RateLimited`, also setting `Retry-After`) and `500` (`StorageFailure` or `ServerFailure`). WebSocket invocation failures use result frames rather than HTTP status codes.

HTTP and WebSocket traffic are unencrypted, and a shared key grants access to the project's registered modules. Review the [deployment trust boundary](<Deployment and Editor Settings.md#management-and-operational-limits>) before exposing the service.
