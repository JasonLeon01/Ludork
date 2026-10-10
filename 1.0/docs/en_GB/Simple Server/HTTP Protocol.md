# HTTP Protocol

The API base is `http://host:7777/<project>/api/v1`. Encode project, account, category and field identifiers separately as URL path segments; routes and names are case-sensitive. The [native client](<Native Client API.md>) constructs these requests internally.

Accounts, categories and fields allow 1–64 UTF-8 bytes without control characters, excluding the exact names `.` and `..`. They may include `/` or `\` when encoded inside their path segment. Projects additionally forbid both slash characters. The server maps project/account/category names to lowercase hexadecimal UTF-8 filenames, avoiding filesystem case folding, reserved names and path traversal. A category file is a dictionary keyed by field name.

## Game data endpoints

| Method and path below the API base | Authentication | Success body |
|---|---|---|
| `GET /health` | None | `{"service":"LudorkServer","project":"MyGame"}` |
| `GET /accounts?sampleCount=10&category=history` | `X-Ludork-Key` | `{"accounts":["player-1"]}` |
| `GET /accounts/:account` | `X-Ludork-Key` | `{"exists":true}` or `{"exists":false}` |
| `GET /accounts/:account/categories/:category/fields?limit=50` | `X-Ludork-Key` | `{"entries":[{"key":"...","value":...}],"nextCursor":null}` |
| `GET /accounts/:account/categories/:category/fields/:field` | `X-Ludork-Key` | `{"found":true,"value":...}` |
| `POST /accounts/:account/write-tokens` | `X-Ludork-Key` | `{"token":"...","expiresAt":...}` |
| `PUT /accounts/:account/categories/:category/fields/:field` | `Authorization: Bearer <token>` | `{}` |

Success responses use HTTP 200 and `Content-Type: application/json`. Account queries, reads, token requests and writes authenticate independently; a successful health response is only a reachability check.

### List accounts

```http
GET /MyGame/api/v1/accounts?sampleCount=10&category=history
X-Ludork-Key: project-key
```

```json
{"accounts":["player-2","player-1"]}
```

Both query parameters are optional:

| Parameter | Meaning |
|---|---|
| `sampleCount` | Omitted or `0`: all eligible accounts in ascending UTF-8 byte order. Positive: sample distinct accounts without replacement, up to the requested count, in random order. Accepted decimal integers range from `0` to `9223372036854775807`. |
| `category` | Only include accounts with this category present as a regular file; apply the filter before sampling. Encode its value as a query parameter. |

If fewer accounts qualify than requested, return all qualifying accounts. No matches succeeds with `{"accounts":[]}`. Category filtering checks file presence, not JSON validity: malformed content still qualifies, but a later read fails with `StorageFailure`. Non-file category paths and inspection errors also fail the query. No accounts or files are created.

The serialized response may not exceed 2 MiB. An all-account query or an oversized sample returns HTTP 400 `InvalidArgument` with `The account list exceeds 2 MiB. Request a smaller positive sampleCount.` It does not truncate the list or return an account cursor. Unknown, repeated, empty or invalid query parameters are rejected.

### Check account existence

```http
GET /MyGame/api/v1/accounts/player-1
X-Ludork-Key: project-key
```

```json
{"exists":false}
```

`exists` is true when the account data directory exists, including an empty directory, consistent with the management account list. A missing directory returns HTTP 200 with `exists: false`. A non-directory account path or inspection failure returns HTTP 500 `StorageFailure`; it is not reported as absence. This query does not inspect category-file contents.

Account queries, field reads and token issuance never create accounts. The first write creates the account directory as needed. A query neither reserves the ID nor atomically prevents a later collision: another client may write after it. Clients generate UUID identifiers themselves; there is no UUID-generation endpoint.

### List category fields

```http
GET /MyGame/api/v1/accounts/player-1/categories/history/fields?limit=2
X-Ludork-Key: project-key
```

```json
{
  "entries": [
    {"key":"2026-10-10T12:30:00.123Z","value":{"score":100}},
    {"key":"2026-10-10T12:20:00.123Z","value":null}
  ],
  "nextCursor":"2026-10-10T12:20:00.123Z"
}
```

`limit` defaults to 50 and accepts integers from 1 to 100. Fields are ordered by descending UTF-8 bytes. The optional `before` is an exclusive upper bound: return only keys whose UTF-8 bytes are smaller, whether or not that boundary key exists. It follows the field-name rules and must be URL-encoded as a query value. Omit it for the first page; an empty value is invalid.

If more fields remain, `nextCursor` is the last returned key. Pass it unchanged as the next `before`, for example `?before=2026-10-10T12%3A20%3A00.123Z&limit=2`. Otherwise `nextCursor` is JSON null. Missing accounts/categories and exhausted ranges succeed with `{"entries":[],"nextCursor":null}`; malformed category contents return `StorageFailure`. Unknown or repeated query parameters are rejected.

Each page reads the current category; pagination provides no snapshot across concurrent writes. Newly inserted keys larger than the current cursor require a refresh from the first page to enter the traversal. Generate fixed-width timestamp keys in your application if byte order should represent time, using one format and time zone. The server does not generate timestamps or sort field values; reusing a key replaces its value. Listing never creates data. A serialized page above 2 MiB returns HTTP 400 `InvalidArgument` and asks for a smaller limit.

### Read a field

```http
GET /MyGame/api/v1/accounts/player-1/categories/profile/fields/score
X-Ludork-Key: project-key
```

```json
{"found":true,"value":100}
```

A missing account, category or field returns `{"found":false,"value":null}`. Stored null returns `{"found":true,"value":null}`. Reading does not create files.

### Acquire and use a token

```http
POST /MyGame/api/v1/accounts/player-1/write-tokens
X-Ludork-Key: project-key
```

The response contains a cryptographically random opaque token and `expiresAt`, a Unix timestamp in **milliseconds**. It is bound to the project/account, valid for 60 seconds from issuance and stored only in memory. Use it for one write:

```http
PUT /MyGame/api/v1/accounts/player-1/categories/profile/fields/score
Authorization: Bearer <token>
Content-Type: application/json

{"value":100}
```

The body must be a JSON object with its own `value` member. Values can be JSON null, booleans, numbers, strings, arrays or objects. Null is a stored value, not deletion. The backend validates the request, synchronously consumes the matching token, checks the write interval, then awaits storage; concurrent uses cannot both succeed. A storage failure or HTTP 429 still consumes the token. Wrong-project/account requests do not consume a valid token belonging elsewhere. Restarting the backend invalidates all tokens.

Requests and complete category files are limited to **1 MiB**, including the file's terminating newline. Updates to one file are serialized, so concurrent writes to different fields do not overwrite each other's changes. Saving replaces a temporary file; unrelated files can update independently. There are no cross-file transactions, delete endpoints or automatic backups. If a write response is lost, read first and acquire a new token only if another write is needed.

Node.js 24 parses original numeric literals and serializes them with native `JSON.rawJSON`. The backend retains integer digits, including signed and unsigned 64-bit values. The viewer uses the same native facility where available and otherwise displays the original JSON text, avoiding rounded display values. Ordinary object fields such as `rawJSON`, `isLosslessNumber` and `__proto__` remain ordinary data.

### Write rate limits

The default write interval is 180 seconds per **project and TCP client IP**, shared by PUT requests across every account/category in that project. IPv4-mapped IPv6 addresses are normalized to IPv4. `X-Forwarded-For` is ignored, so clients behind the same NAT or reverse proxy can share a write allowance. Configure `ipWriteIntervalSeconds` as described in [Deployment and Editor Settings](<Deployment and Editor Settings.md#configure-the-write-interval>); `0` disables the interval.

Validation or authentication failures before admission do not start the interval. Once admitted, the write starts it before storage and uses the allowance even if storage later fails. Rejected writes do not extend the existing deadline. Reads, lists, account checks, health checks, token issuance and management requests do not count. Deadlines are held in memory and reset on backend restart.

An early PUT with a valid token returns:

```http
HTTP/1.1 429 Too Many Requests
Content-Type: application/json
Retry-After: 120

{"code":"RateLimited","message":"This IP address must wait before writing to this project again.","retryAfterSeconds":120}
```

The JSON `retryAfterSeconds` and `Retry-After` header contain the same remaining delay, rounded up to whole seconds. This optional JSON property is supplied only for `RateLimited`. The valid token is already consumed before the limit check. Wait at least that delay, then acquire a **new** token when ready to write; do not reuse the old token or acquire one before a long wait. The client does not retry automatically.

## Read-only management

Management paths are under `<project>/api/v1/admin`. Vite on port 3333 proxies these requests to port 7777; the backend enforces authentication. It does not enable CORS.

| Method and path below `/admin` | Requirement | Success body |
|---|---|---|
| `POST /login` | JSON `{"key":"...","password":"..."}` and `X-Ludork-Admin: 1` | `{"project":"MyGame"}`, plus session cookie |
| `GET /session` | Session cookie | `{"project":"MyGame"}` |
| `POST /logout` | Session cookie and `X-Ludork-Admin: 1` | `{}`, clears session cookie |
| `GET /accounts` | Session cookie | `{"accounts":["player-1"]}` |
| `GET /accounts/:account/categories` | Session cookie | `{"categories":["profile"]}` |
| `GET /accounts/:account/categories/:category` | Session cookie | The category dictionary |

The session is project-bound, expires after eight hours and is invalidated by logout or backend restart. Cookies are HttpOnly, SameSite=Strict and restricted to that project's management API path. Management has no write/delete endpoint. Responses use `Cache-Control: no-store`; the frontend does not store credentials in browser storage.

## Errors and trust boundary

Non-2xx responses contain a stable code and diagnostic message:

```json
{"code":"AuthenticationFailed","message":"Project credentials are invalid."}
```

| HTTP status | Code and typical cause |
|---|---|
| `400` | `InvalidArgument`: invalid names, JSON, encoding or size. |
| `401` | `AuthenticationFailed` or `InvalidWriteToken`. |
| `403` | `AuthenticationFailed`: missing management request header. |
| `404` | `InvalidArgument`: unknown project/endpoint or missing management category. |
| `429` | `RateLimited`: the project/IP write interval has not elapsed; includes `retryAfterSeconds` and `Retry-After`. |
| `500` | `StorageFailure` or `ServerFailure`. |

This protocol sends keys, tokens, cookies and data over plaintext HTTP. A project key permits access to every account in the project; it is not account authentication, and distributing the key inside a client does not protect it from extraction. See [deployment limits](<Deployment and Editor Settings.md#management-and-operational-limits>) before exposing the service outside a trusted network.
