# Ludork Server

Ludork's optional Node.js 24 server runs project logic in Lua 5.4 through the pinned Wasmoon runtime. Node owns connections, credentials, storage and process supervision. Lua modules own snapshots, RPC handlers and online notifications. Each project has a separate Lua process. The independent `Server` branch contains the deployable service; the client engine lives on the main development branch.

## Deploy

Extract the built `LudorkServer-<version>.tar.gz` package to a deployment directory. Install **Node.js 24** with npm, then deploy a project:

```sh
tar -xzf LudorkServer-1.0.0.tar.gz
cd LudorkServer
sh deploy.sh MyGame 'project-key' 'management-password'
sh start.sh
```

On Windows, use Command Prompt:

```bat
deploy.bat MyGame "project-key" "management-password"
start.bat
```

Quote arguments for your shell, particularly `$`, `%`, `!`, `&` and quote characters. Deployment arguments can remain in shell history and process listings. Do not put production credentials in source-controlled scripts.

Deployment runs `npm ci --omit=dev`, saves credential hashes in `config/projects.json`, and creates a project Lua entry. The package already contains compiled backend JavaScript, built frontend assets, and `start.sh` / `start.bat`; deployment does not compile code or install development dependencies. Wasmoon ships its Lua VM as WebAssembly; no system Lua installation or native compiler is needed. The start script supervises the backend and production management server together; Ctrl+C stops both. A failed service process stops the other service. Occupied ports cause startup to fail.

| Purpose | Address |
| --- | --- |
| Read-only management website | `http://IP:3333/MyGame/` |
| Ludork client server URL | `http://IP:7777/MyGame` |
| Client WebSocket endpoint | `ws://IP:7777/MyGame/api/v1/net` |

For a domain, set `LUDORK_ALLOWED_HOSTS=games.example.com` before starting. Multiple exact hostnames are comma-separated, without scheme or port. IP addresses, `localhost` and names ending in `.localhost` work directly. Both services listen on all interfaces; deployment does not configure a firewall, TLS, system service or automatic startup.

Deploy another project to add it to the same instance. Redeployment rotates that project's credentials while preserving data, Lua source, `luaEntry` and `ipWriteIntervalSeconds`. Deploy projects sequentially and stop the service while reinstalling dependencies. Restart to apply configuration or Lua source changes.

Project names support 1–64 UTF-8 bytes, excluding control characters, slash, backslash, and the exact names `.` and `..`. Keys use 1–4096 visible ASCII characters without spaces. Passwords support 1–4096 UTF-8 bytes without control characters. There are no default credentials.

## Build from source

The `Server` branch owns the build inputs. Install Node.js 24 and run:

```sh
git clone --branch Server --single-branch https://github.com/JasonLeon01/Ludork.git LudorkServerSource
cd LudorkServerSource
npm ci
npm run build
```

The fixed output is `dist/LudorkServer/`. Copy that directory to a separate deployment location, then run its deploy and start scripts as described above. Source checkout deploy/start commands reject direct execution. Building never reads private project settings, data or Lua logic into the package; if the output directory already contains `config/`, `data/`, `projects/` or `logs/`, the build refuses to replace it. Move the deployed instance outside `dist` before building again.

The build compiles TypeScript, bundles React with Vite, copies Lua runtime files and declarations, and writes a production-only package manifest and lockfile. The deployable directory contains no TypeScript/TSX sources, development dependencies, `node_modules`, or private deployment data. Run `npm run typecheck` and `npm run lint` when changing source. Lua declarations and the standalone EmmyLua workspace describe the server-only language surface.

## Project Lua logic

Deployment creates `projects/<hex-project-name>/Main.lua`. The directory name is the lowercase hexadecimal encoding of the project's UTF-8 bytes; deployment prints the exact path. This private directory is ignored by Git. The per-project `luaEntry` configuration defaults to `Main.lua` and can select another relative `.lua` path within this directory. Redeployment never overwrites an existing entry.

The initial entry registers the supplied storage module:

```lua
local Net = require("Framework.Net")
local Storage = require("Modules.Storage")

Net.RegisterModule("Storage", Storage)
```

Put a custom module in the project directory and register it from the entry. Framework modules are loaded from `lua/`; project modules are loaded from the project's own directory. Declaration files and `stub` directories are excluded from the runtime. Server source is never sent to clients. Server logic uses ordinary Lua 5.4; the client's engine-specific Lua classes and native modules are unavailable here.

For example, a project `Profile.lua` can return:

```lua
local Profile = {Rpc = {}}

function Profile.Snapshot(context, params)
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

Register it with `Net.RegisterModule("Profile", require("Profile"))`. Module, RPC and event names are case-sensitive and contain 1–128 ASCII letters, digits, dots, underscores or hyphens. Duplicate module registration fails project initialization. Only functions explicitly placed in `Rpc` are remotely callable.

`Snapshot(context, params)` receives the client's pure Lua parameter value and must return an ordinary table. Snapshot storage access is read-only. Every fetch executes this function again. The server does not retain proxy objects or automatically replicate changes.

`Rpc.FunctionName(context, ...)` receives positional arguments, including nil gaps and trailing nil values. RPCs are void: returned values are discarded. Host storage operations suspend the current Lua coroutine until Node finishes the operation, so the Lua code can use normal sequential calls. Do not retain a context or use its services after its handler finishes. Calls are serialized within each project.

The host supplies these context members:

| Member | Meaning |
| --- | --- |
| `ProjectName`, `ModuleName` | Current project and registered module |
| `SessionId` | Opaque ID of the current online connection |
| `PeerAddress` | Actual TCP peer address; forwarded headers are ignored |
| `Storage` | Storage services restricted to the current project |
| `Notify(event, ...)` | Queue an event for the current connection |
| `Send(sessionId, event, ...)` | Queue an event for one online connection in this project |
| `Broadcast(event, ...)` | Queue an event for all online connections in this project |
| `AdmitWrite(account, category, field)` | Apply the configured interval inside the supplied `Storage.WriteField` RPC |

Call context methods with `:`. Notifications use the current module name and are flushed only after the handler succeeds. Failed handlers discard their queued events. Events are online-only: disconnected clients receive no backlog and reconnecting does not replay events. Receiving an event does not update any client proxy; client business code decides whether to fetch or sync.

An ordinary Lua error fails that call with `ServerFailure`. A business handler can raise `{code = "InvalidArgument", message = "..."}` for a rejected argument. Host storage and rate errors preserve their stable codes. A Lua process crash, execution timeout or memory failure invalidates that project's online connections and restarts its Lua state; pending RPCs are never replayed. Other projects, HTTP health checks and management remain available. A project whose entry fails to load rejects requests until its source is fixed and the service restarted.

## Client snapshots and RPC

Enable server integration in a Ludork C++ Source project's editor settings or packaging configuration. Lua never supplies the URL or project key. Standalone projects do not expose server configuration.

The client Net module fetches a new Lua proxy for each `Net.GetProxy(moduleName, params, callback)` request. Its authored getter methods read the local snapshot. `proxy:Sync(callback)` fetches the same module and captured parameters again, replacing that object's snapshot only on success. Separate proxy instances remain independent. No getter performs network I/O, and neither RPC completion nor events implicitly refresh a snapshot.

The client's network-module handle calls `CallRpcFunction("FunctionName", ...)`. RPCs have no business return value. Errors are reported through the client's networking error path; server notifications are subscribed through the client event API. The engine documentation owns the complete client signatures and lifecycle.

## Storage and supplied module

Each account/category file is a versioned, data-only `.ldata` document. Its root is a dictionary of field names to Lua values. The file contains the same tagged value encoding used on the wire; it is never executed as Lua source. Old `.json` files are not read, converted or deleted. This development-time format change has no compatibility reader or automatic migration.

Account, category and field names support 1–64 UTF-8 bytes without control characters; the exact names `.` and `..` are invalid. Encoded filenames preserve case and cannot escape the data directory. Account IDs are supplied by game logic; the service does not generate or authenticate them.

| Lua storage method | Result |
| --- | --- |
| `context.Storage:AccountExists(account)` | Boolean; reads do not create accounts |
| `context.Storage:ListAccounts(sampleCount, categoryFilter)` | Array of account names |
| `context.Storage:ReadField(account, category, field)` | `{found = boolean, value = value}`; missing values are nil |
| `context.Storage:ListFields(account, category, beforeKey, limit)` | `{entries = {{key = key, value = value}, ...}, nextCursor = cursor}` |
| `context.Storage:WriteField(account, category, field, value)` | No return value; nil removes the field |

Missing accounts/categories produce empty lists or `found = false`. `sampleCount` is a nonnegative signed 64-bit integer: nil or zero lists every matching account in ascending UTF-8 byte order; a positive value samples unique accounts. A category filter checks for that category's `.ldata` file before sampling. `beforeKey` is an exclusive upper bound for descending UTF-8 field order. Nil starts the first page. `limit` defaults to 50 and accepts 1–100. `nextCursor` is nil when no more matching fields remain. Pagination observes current files, without a snapshot across requests.

The supplied `Storage` module's snapshot parameters are:

| `params.query` | Other parameter fields | Snapshot |
| --- | --- | --- |
| `account_exists` | `account` | `{exists = boolean}` |
| `accounts` | Optional `sampleCount`, `categoryFilter` | `{accounts = {...}}` |
| `field` | `account`, `category`, `field` | `{found = boolean, value = value}` |
| `fields` | `account`, `category`; optional `beforeKey`, `limit` | `{entries = {...}, nextCursor = cursor}` |

Its `WriteField(account, category, field, value)` RPC replaces the old token/PUT flow. On success it notifies the caller with `FieldWritten(account, category, field)`. There is no write-token API. A nil value deletes the field but does not delete the category file or account directory.

The project setting `ipWriteIntervalSeconds` applies only to this supplied `Storage.WriteField` RPC. It defaults to 180; zero disables it. The value must be a nonnegative safe integer. The first admitted attempt starts the interval before storage; storage failure still consumes it. Rejection returns `RateLimited` with positive `retryAfterSeconds` and does not extend the interval. Limits are isolated by project, normalize IPv4-mapped IPv6, use a monotonic clock and reset on service restart. Clients behind a common NAT or reverse proxy share its peer-IP interval. Custom RPCs are responsible for their own business rules and do not inherit this interval.

Writes create account/category storage as needed. Same-file updates are serialized and use atomic temporary-file replacement. Different files can update concurrently. Each category is limited to 1 MiB including encoding. There are no cross-file transactions, automatic backups or rollback of earlier writes when a later Lua statement fails. A timeout or disconnect can occur after a write committed; do not automatically retry an RPC with an unknown outcome. Fetch a new snapshot and let business logic decide the next action.

## Network protocol

Connect to `/<URL-encoded-project>/api/v1/net` with `X-Ludork-Key` in the HTTP WebSocket upgrade. The server replies with `{"type":"ready","version":1}`. Frames are UTF-8 JSON text. Credentials never occur in URLs or message bodies.

```text
snapshot: {type:"snapshot", id:"1", module:"Profile", params:WireValue}
RPC:      {type:"rpc", id:"2", module:"Profile", name:"SetName", args:[WireValue,...]}
success:  {type:"result", id:"1", ok:true, data:WireValue}
RPC ack:  {type:"result", id:"2", ok:true}
failure:  {type:"result", id:"2", ok:false, code:"InvalidArgument", message:"..."}
event:    {type:"event", module:"Profile", event:"NameChanged", args:[WireValue,...]}
```

IDs must be unique among pending requests on a connection. A rate failure also includes `retryAfterSeconds`. RPC acknowledgments report transport/execution success without a function return value. The only unauthenticated client HTTP endpoint is `GET /<project>/api/v1/health`, returning `{"service":"LudorkServer","project":"..."}`. Previous account/field REST endpoints and tokens have been removed.

`WireValue` is one of:

```text
["nil"]
["bool", true]
["int", "9223372036854775807"]
["float", "1.2345678901234567"]
["string", "Ynl0ZXM="]
["table", [[WireKey, WireValue], ...]]
```

Integers are exact signed 64-bit decimal strings. Floats are finite double-precision numbers encoded as round-trip decimal strings. Strings contain arbitrary bytes encoded as canonical base64. Tables can have mixed or sparse boolean, numeric and string keys. Nil is supported as a parameter or value, but a table entry with nil means absence and is not encoded. Equivalent keys such as integer `1` and float `1.0` cannot occur twice. Metatables, cycles, table keys, functions, threads and userdata are rejected. Repeated acyclic tables are copied; object identity is not preserved.

Messages are limited to 2 MiB, data to 64 levels of nesting and 65,536 value nodes. Incoming queues are capped at 256 calls and 8 MiB per connection and project; outgoing buffering is capped at 8 MiB per connection. Each invocation has a 10-second overall deadline, including queue time; Lua execution has a 9-second interrupt budget and a 64 MiB allocator limit. Slow connections are closed. WebSocket ping/pong checks remove dead connections.

Stable server failures include `InvalidArgument`, `AuthenticationFailed`, `RateLimited`, `StorageFailure`, `ServerFailure` and `Timeout`. Clients additionally distinguish local disabled, connectivity and malformed-response failures. Use codes rather than message wording.

## Read-only management and operations

Open `http://host:3333/<project>/` and supply the project key and management password. Password hashes use scrypt with a per-project random salt. The project-specific HttpOnly, SameSite=Strict cookie lasts eight hours; restarting invalidates sessions. Login/logout require a custom header, and the server does not enable CORS.

The viewer lists accounts and `.ldata` categories and displays typed Lua values, including exact integers and escaped byte strings. It has no editing or deletion UI. The production management server serves only the built page and frontend assets, and proxies project API requests to the backend. Project logic, data, configuration and backend files are not served. Management data endpoints require the project's session.

This service retains shared project-key authentication. Account IDs are storage namespaces, not authenticated identities. An extracted client key can access that project's registered network modules. HTTP/WebSocket traffic is unencrypted; use the intended network boundary or a separately managed TLS proxy. This service does not configure that infrastructure.

```text
backend/                   Compiled JavaScript networking and host services
lua/                       Lua framework, supplied modules and entry template
frontend/                  Built management page and static assets
projects/<project-hex>/     Private project Lua source (ignored)
config/projects.json       Project settings and credential hashes (ignored)
data/<project>/<account>/  Encoded category .ldata files (ignored)
data/.server.lock          Single-backend lock (ignored)
start.sh / start.bat       Packaged service launchers
```

Back up private project logic, configuration and data through your operational process. Restrict filesystem access to the service administrator. The instance lock detects a live recorded PID without killing it; an exited process's stale lock can be recovered. For malformed locks or reused PIDs, confirm no backend is running before manually removing the lock.

Built archives exclude private logic, credentials, user data and installed dependencies. Their runtime package and lockfile install only the required production dependencies.

Copyright and license texts for Wasmoon, its embedded Lua runtime, ws and the bundled React components are included in [Third-party notices](third-party/README.md).
