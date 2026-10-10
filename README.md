# Ludork Server

Ludork's optional, self-hosted HTTP data store. It stores a dictionary in each account/category JSON file and includes a read-only React/TypeScript viewer. It does not provide multiplayer synchronization, RPC, account login, or HTTPS. The client engine is maintained on Ludork's main development branch; this independent `Server` branch contains only the deployable server.

## Deploy

Install **Node.js 24** (with npm), then clone this branch onto the server:

```sh
git clone --branch Server --single-branch https://github.com/JasonLeon01/Ludork.git LudorkServer
cd LudorkServer
sh deploy.sh MyGame 'project-key' 'management-password'
sh start.sh
```

On Windows, use Command Prompt:

```bat
deploy.bat MyGame "project-key" "management-password"
start.bat
```

Quote and escape arguments for your shell, particularly `$`, `%`, `!`, `&` and quote characters. Deployment arguments may remain in shell history and process listings. Do not put real credentials into source-controlled scripts.

Deployment checks Node/npm, runs `npm ci`, saves credential hashes in `config/projects.json`, and generates `start.sh` and `start.bat`. It **does not build the frontend**. The start script supervises the backend and Vite together; Ctrl+C stops both. A failed child process stops the other process. Occupied ports cause startup to fail rather than switch to another port.

| Purpose | Address |
| --- | --- |
| Read-only management website | `http://IP:3333/MyGame/` |
| Ludork client server URL | `http://IP:7777/MyGame` |

IP addresses work directly. For a domain, list the permitted hostname before starting Vite:

```sh
export LUDORK_ALLOWED_HOSTS=games.example.com
sh start.sh
```

```bat
set LUDORK_ALLOWED_HOSTS=games.example.com
start.bat
```

Multiple hostnames are comma-separated, without a scheme or port. Node listens on all interfaces; configure your own network/firewall access. Deployment does not install a system service or configure automatic startup.

Deploy another project with the same command to add it to the same instance. Redeploying an existing project updates its key/password and preserves its data. Restart the service to load configuration changes. Deploy projects sequentially and stop the service while reinstalling dependencies. Each project has independent credentials, data, tokens, and management sessions.

Each project object in `config/projects.json` has an `ipWriteIntervalSeconds` setting. New deployments write `180`; an omitted setting also defaults to 180 seconds. Set it to `0` to disable the write interval, or another nonnegative safe integer (up to `9007199254740991`) to change it. Fractional, negative, string, and null values are rejected. Redeployment preserves an existing setting and user data; restart to apply edits.

Project names support 1–64 UTF-8 bytes, excluding control characters, slash, backslash, and the exact names `.` and `..`. URL-encode the project as one path segment; Chinese project names are supported. Keys use 1–4096 visible ASCII characters without spaces. Passwords support 1–4096 UTF-8 bytes without control characters. No default credentials are supplied.

## Client API

In Ludork C++ Source projects, enable the server in the editor's test-server settings or packaging dialog and configure the backend URL and project key. Lua never supplies the URL or key. The native `GlobalCore.Server` API is:

| Method | Completion result |
| --- | --- |
| `CheckAccountExistsAsync(accountId, onCompleted)` | `ServerAccountResult` |
| `ListAccountsAsync(sampleCount, categoryFilter, onCompleted)` | `ServerAccountListResult` |
| `ReadFieldAsync(accountId, category, field, onCompleted)` | `ServerReadResult` |
| `ListFieldsAsync(accountId, category, beforeKey, limit, onCompleted)` | `ServerFieldListResult` |
| `AcquireWriteTokenAsync(accountId, onCompleted)` | `ServerTokenResult` |
| `WriteFieldAsync(writeToken, category, field, value, onCompleted)` | `ServerResult` |
| `CheckConnectionAsync(onCompleted)` | `ServerConnectionResult` |

The engine documentation describes the generated Lua types, asynchronous operation lifecycle, network enum, and build-time configuration. Standalone projects do not expose server configuration.

The caller supplies the account ID, for example a caller-generated UUID. `ServerAccountResult` inherits `ok`, `code`, and `message` from `ServerResult` and adds the read-only `exists` flag. Inspect `exists` only when `ok` is true.

`ServerAccountListResult` exposes the `accounts` string array. `ServerFieldListResult.entries` contains read-only `ServerFieldEntry` objects with `key` and `value`, and its `nextCursor` is a string or nil. Pass explicit nil placeholders for optional arguments before the required completion callback: nil `sampleCount` lists all accounts, nil `categoryFilter` skips filtering, nil `beforeKey` starts at the largest key, and nil `limit` selects 50 entries. `ServerResult.retryAfterSeconds` is present only for a rate-limited write.

## HTTP protocol

All endpoints are relative to `http://host:7777/<project>/api/v1`. Encode each account, category, and field as a separate URL path segment. Names support 1–64 UTF-8 bytes without control characters; the exact names `.` and `..` are invalid. Names are case-sensitive and stored using lowercase hexadecimal UTF-8 filenames, so Unicode, slash characters and Windows reserved names cannot escape the data directory or collide by filesystem case folding.

### Check whether an account exists

```http
GET /MyGame/api/v1/accounts/player-1
X-Ludork-Key: project-key
```

Success: HTTP 200 with `{"exists":true}` or `{"exists":false}`. An account exists when its persisted account directory exists, matching the management account list. A missing account is a successful query with `exists:false`; filesystem failures or a non-directory account path return `StorageFailure`.

Checking an account, reading a field, and acquiring a write token do not create the account directory. A write creates it as needed. This query observes current storage only: it does not reserve an ID or guarantee uniqueness between a check and a later write. Generate UUIDs in the caller; the server does not generate or require UUID-formatted account IDs.

### List or sample accounts

```http
GET /MyGame/api/v1/accounts?sampleCount=10&category=messages
X-Ludork-Key: project-key
```

Success: `{"accounts":["player-1","player-2"]}`. Omit `sampleCount` or use `0` to return every account, sorted by ascending UTF-8 byte order. A positive signed 64-bit integer selects that many unique random accounts, or all eligible accounts if fewer exist. An optional `category` includes only accounts with that category file; filtering happens before sampling and does not parse file contents. A directory or other invalid filesystem object at the category file path returns `StorageFailure`.

The complete serialized response, including JSON escaping and its envelope, cannot exceed 2 MiB. Oversized full lists or samples return HTTP 400 with `InvalidArgument` and a message requesting a smaller positive `sampleCount`; results are never silently truncated. An empty project or filter match returns `{"accounts":[]}`. Empty, negative, fractional, repeated, nested, or unsupported query parameters are rejected.

### Read a field

```http
GET /MyGame/api/v1/accounts/player-1/categories/profile/fields/name
X-Ludork-Key: project-key
```

Success: `{"found":true,"value":"Hero"}`. A missing account, category or field returns `{"found":false,"value":null}`. A stored null returns `{"found":true,"value":null}`.

### List fields with a cursor

```http
GET /MyGame/api/v1/accounts/player-1/categories/messages/fields?before=20261010120000&limit=50
X-Ludork-Key: project-key
```

Success: `{"entries":[{"key":"20261010115959","value":{"text":"Hello"}}],"nextCursor":null}`. Fields are ordered by descending UTF-8 bytes, not numeric value or insertion time. `before` is an exclusive upper bound: only keys smaller than the cursor are returned, whether or not that key currently exists. Omit `before` for the first page. `limit` defaults to 50 and accepts integers from 1 through 100.

`nextCursor` is the last returned key only when more matching fields remain; otherwise it is null. Missing accounts/categories and exhausted ranges return `{"entries":[],"nextCursor":null}`. Values retain their JSON types and exact numeric digits. Empty, malformed, repeated, nested, or unsupported query parameters are rejected. Each request observes the current file; pagination does not reserve a snapshot across concurrent writes.

For chronological lists, generate fixed-width time keys in the caller so byte order matches time order. Writing an existing key through `WriteFieldAsync` overwrites its value. Newly inserted larger keys appear when you refresh the first page, rather than in later pages bounded by an older cursor.

### Acquire a one-use write token

```http
POST /MyGame/api/v1/accounts/player-1/write-tokens
X-Ludork-Key: project-key
```

Success: `{"token":"<opaque-token>","expiresAt":1790000060000}`. `expiresAt` is Unix epoch milliseconds. A token is valid for **60 seconds** from issuance, for one write to its bound project/account. Tokens are generated using cryptographic randomness and kept only in memory; a server restart invalidates them.

### Write a field

```http
PUT /MyGame/api/v1/accounts/player-1/categories/profile/fields/name
Authorization: Bearer <opaque-token>
Content-Type: application/json

{"value":"Hero"}
```

Success: `{}` with HTTP 200. The backend consumes the token synchronously after validating the request and before awaiting storage; concurrent uses cannot both succeed. A storage failure still consumes the token. Wrong-account/project requests do not consume a valid token belonging elsewhere. Do not automatically retry timed-out writes: read the field first, then acquire a new token if another write is necessary.

After consuming a valid token, the backend applies the project's write interval to the actual TCP peer IP. The first admitted write starts the interval synchronously, so concurrent requests cannot bypass it by changing accounts or categories. An admitted attempt still consumes the interval if storage fails. A rejected write does not extend the interval, but its valid one-use token is consumed. Reads, lists, connection checks, and token acquisition do not consume the write interval.

A request during the interval returns HTTP 429 with `{"code":"RateLimited","message":"...","retryAfterSeconds":180}` and a matching `Retry-After: 180` header. The delay is rounded up to a positive whole second. Wait before acquiring a new token and retrying explicitly. Limits are isolated by project, normalize IPv4-mapped IPv6 addresses, use a monotonic clock, and reset on backend restart. `Forwarded` and `X-Forwarded-For` are ignored: clients sharing a NAT or reverse-proxy peer IP share that project's interval.

Each write updates one top-level dictionary field; it creates the account/category as needed. Values may be JSON strings, booleans, numbers, null, arrays, or nested objects. Null is a value, not a delete operation. Requests and each category file are limited to 1 MiB; updates to the same file are serialized and saved by replacing a temporary file. Distinct files can update concurrently. There are no multi-field transactions, cross-file transactions, delete API, or automatic backups.

The server preserves numeric source text using Node.js 24's native JSON reviver context and `JSON.rawJSON`. Signed and unsigned 64-bit integers retain their exact digits, and ordinary dictionary keys such as `__proto__` remain data. The viewer uses the same standard when formatting JSON; browsers without this feature display the original JSON text to preserve precision.

### Connection probe

```http
GET /MyGame/api/v1/health
```

Success: `{"service":"LudorkServer","project":"MyGame"}`. This lightweight endpoint needs no credentials and reveals no account data. Ludork reports `Normal` at ≤300 ms, `Weak` above 300 ms through 3 seconds, and `Disconnected` if no valid response arrives in 3 seconds. It tests this server's reachability, not general Internet availability or key validity.

### Errors

Errors return `{"code":"AuthenticationFailed","message":"Project credentials are invalid."}` with an appropriate non-2xx HTTP status. Callers use the stable code instead of parsing message text:

- `InvalidArgument`: invalid JSON, identifiers, body size, or endpoint.
- `AuthenticationFailed`: project key or management credentials/session rejected.
- `InvalidWriteToken`: missing, wrong, expired, or already used write token.
- `RateLimited`: the peer IP must wait before its next write to this project; `retryAfterSeconds` and `Retry-After` carry the same delay.
- `StorageFailure`: data could not be read or saved.
- `ServerFailure`: unexpected server error.

The engine additionally reports `Disabled`, `Timeout`, `ConnectionFailed`, and `InvalidResponse` for local failures; `None` denotes success. No credentials are included in URLs or server application logs.

## Read-only management

Open `http://host:3333/<project>/`, then supply both the project key and management password. Password hashes use scrypt with a per-project random salt. The browser receives a project-specific HttpOnly, SameSite=Strict session cookie, valid for eight hours; restarting the backend invalidates sessions. Login/logout require a custom request header, and the backend does not enable CORS.

The viewer lists accounts and classification files, filters accounts, displays formatted JSON, refreshes the current selection, and logs out. There is no editing or deletion UI. The Vite server serves only frontend inputs and imported frontend dependencies; it cannot serve `data/`, `config/`, backend code, or deployment scripts. Management requests are proxied to the backend, where every data endpoint checks the session.

This design is intended for a trusted internal network. **HTTP transmits credentials and data in plaintext.** A shared project key grants access to every account in that project; account IDs are storage namespaces, not independently authenticated identities. A key embedded in a distributed client can be extracted. One-use tokens prevent replay but do not make an untrusted client authoritative. Restrict network access or provide TLS at a separately managed reverse proxy when needed; this project does not configure that infrastructure.

## Files and operations

```text
backend/                  Node.js TypeScript backend source
frontend/                 React/TypeScript viewer and Vite configuration
tools/                    Shared deployment and process supervision
config/projects.json      Generated project credential hashes (ignored)
data/<project>/<account>/  Encoded category JSON files (ignored)
data/.server.lock         Single-backend lock (ignored)
start.sh / start.bat       Generated launchers (ignored)
```

Back up `config/` and `data/` through your own operational process. Local filesystem access to these directories must be restricted to the service administrator. The instance lock checks whether its recorded PID is still alive without killing it. An exited process's stale lock can be recovered automatically. If a lock is malformed or its PID has been reused, first confirm no instance is running, then remove `data/.server.lock` manually; the server refuses to guess or kill another process.

Before checking in or archiving source, ensure generated configuration, user data, logs, dependencies, and launchers remain ignored. The source dependency lockfile is committed by the repository owner; run `npm ci` to reproduce dependencies.

Development checks (no frontend production build):

```sh
npm ci
npm run typecheck
npm run lint
```
