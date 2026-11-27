# Engine: Events and Module Root

This page documents asynchronous operations (`AsyncOperation`), pub/sub messaging (`EventBus`), the `Engine` module root queries and constants, resource-file constants, editor live debugging, native web views, typed/JSON/LDC data I/O and asynchronous save previews (`SavePreviewReader`). Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; constructors, static methods and module-level functions take no receiver.

## AsyncOperation

`Engine.AsyncOperation` represents asynchronous work shared by Lua and Blueprint. Lua callers wait with `asyncio.await(operation)` inside a cooperative task, while Blueprint latent nodes resume through their declared stages.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `new` | `poll?: function(operation: Engine.AsyncOperation)` | `Engine.AsyncOperation` | Static. Creates a pending operation. Optional `poll(operation)` runs once per logic update until completion or cancellation. |
| `emit` | `value: any` | — | Publishes an intermediate Blueprint stage without completing Lua waiters. |
| `complete` | `result?: any` | — | Completes with one retained result, also used as the final Blueprint stage value (`nil`, `false` and `0` are valid results). |
| `cancel` | — | — | Cancels the operation and its waiting executions. Completed or cancelled operations stay unchanged. |
| `getStatus` | — | `string` | Returns read-only constants `AsyncOperation.PENDING` (`"pending"`), `COMPLETED` (`"completed"`) or `CANCELLED` (`"cancelled"`). |
| `getResult` | — | `any` | Reads the completed result; pending or cancelled operations raise an error. |

Execution and cleanup semantics:

- Multiple waiters can share one operation without consuming one another's stages. A completed operation can be awaited repeatedly; new Blueprint waiters receive only its final result.
- `poll` callbacks run on the logic thread, must not yield and propagate errors to the host.
- The operation's owner handles resource cleanup; cancellation alone does not undo business side effects. See [Lua task usage and cancellation](<../../../../Getting Started/Ludork Lua Advanced.md#cooperative-tasks-and-file-batches>) and [Blueprint cancellation](<../../../Blueprint Scripting/Execution Flow Events and Variables.md#cancelling-latent-execution>).

## EventBus

Provides priority-ordered immediate (`publish`) and queued (`post` / `flush`) event dispatch for Lua callbacks and Blueprint events.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `subscribe` | `event: string`, `handler: function`, `priority: int = 0` | `int` | — |
| `once` | `event: string`, `handler: function`, `priority: int = 0` | `int` | — |
| `subscribeObjectHandler` | `event: string`, `object: any`, `handler: function`, `priority: int = 0` | `int` | — |
| `subscribeBlueprintEvent` | `event: string`, `object: any`, `eventName: string`, `priority: int = 0` | `int` | — |
| `onceBlueprintEvent` | `event: string`, `object: any`, `eventName: string`, `priority: int = 0` | `int` | — |
| `unsubscribe` | `token: int` | `bool` | — |
| `unsubscribeEvent` | `event: string` | `bool` | — |
| `unsubscribeObjectHandler` | `event: string`, `object: any` | `bool` | — |
| `clear` | `event: string = nil` | — | — |
| `publish` | `event: string`, `payload: any = nil` | — | — |
| `post` | `event: string`, `payload: any = nil` | — | — |
| `flush` | `limit: int = nil` | `int` | — |

## Engine

Module-root constants and runtime query functions exposed directly on `require("Engine")`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `ZeroVector2f` | `sf.Vector2f` | — | — |
| `ZeroVector2i` | `sf.Vector2i` | — | — |
| `ZeroVector2u` | `sf.Vector2u` | — | — |
| `ZeroVector3f` | `sf.Vector3f` | — | — |
| `ZeroVector3i` | `sf.Vector3i` | — | — |
| `ZeroVector3u` | `sf.Vector3u` | — | — |
| `Direction` | enum table | — | `DOWN = 0`, `LEFT = 1`, `RIGHT = 2`, `UP = 3`. |
| `FocusDirection` | `any` | — | — |
| `DefaultFontSize` | `int` | — | — |

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `GetCellSize` | — | `int` | Pure. |
| `GetGameSize` | — | `sf.Vector2u` | Pure. |
| `GetScale` | — | `float` | Pure. |
| `IsGameRunning` | — | `bool` | Pure. |
| `OpenWebView` | `url: string` | `accepted: bool` | Ordinary execution; returns when the request is accepted. |
| `C_CompressAnimation` | `frameCount: int`, `frameStep: float`, `frameRate: int`, `timeLines: Engine.AnimationTimeline[]`, `assets: string[]`, `imageFormat: string` | `duration: float`, `frames: string[]`, `sounds: Engine.AnimationSoundEntry[]` | — |

Runtime constants and query semantics:

- **Read-only queries:** `Engine.GetCellSize()` returns the C++ compile-time cell size (currently `32`, with no runtime setter), `GetGameSize()` returns a detached logical-size value, `GetScale()` returns the effective render scale, and `IsGameRunning()` reads lifecycle state. Change runtime state through `GlobalCore.Display` and `GlobalCore.System.exit()`.
- **Directions:** `Engine.Direction` and `require("Enums.Engine.Direction")` expose identical native enum values: `DOWN = 0`, `LEFT = 1`, `RIGHT = 2`, `UP = 3`. Use the named members for Actor facing and `Engine.OppositeDirection(direction)` to get the opposite direction.
- **Scale limits:** `Engine.DisplayScaleLimits` exposes read-only numeric constants: `MINIMUM_RENDER_SCALE` (`0.01`) is the effective render-scale lower bound, and `MINIMUM_INVERTIBLE_SCALE` (`0.000001`) guards against dividing by a near-zero display scale.

## ResourceFileConstants

`Engine.ResourceFileConstants` exposes static, read-only string properties shared by native resource loaders, Lua and packaging tools. The class uses `metadata = false` and does not add Blueprint nodes.

| Field | Type | Default | Notes |
|---|---|---|---|
| `LUA_SOURCE_EXTENSION` | `string` | `".lua"` | Static, read-only. |
| `LUA_COMPILED_EXTENSION` | `string` | `".luac"` | Static, read-only. |
| `DATA_EXTENSION` | `string` | `".json"` | Static, read-only. |
| `ENCRYPTED_DATA_EXTENSION` | `string` | `".ldc"` | Static, read-only. |
| `ANIMATION_CACHE_SUFFIX` | `string` | `".anim.json"` | Static, read-only. |
| `ENCRYPTED_ANIMATION_CACHE_SUFFIX` | `string` | `".anim.ldc"` | Static, read-only. |

## EditorLiveDebug

`Engine.EditorLiveDebug` is a built-in editor bridge service (`metadata = false`, Lua only, no Blueprint nodes).

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `install` | `handler: function` | — | Static. Lua only (`metadata = false`). Installs or replaces the current VM's request handler. |
| `uninstall` | — | — | Static. Lua only (`metadata = false`). Releases the handler; safe to call repeatedly. |

The handler receives one request table and must return exactly one JSON-serialisable table. Missing handlers, Lua errors, invalid return counts and encoding failures produce `success = false` with an `error` string. Registration belongs to the Lua VM and is released on shutdown; installation does nothing without an editor bridge.

> **Note:** The default Game module owns Live Debug handler registration; project scripts should not replace it during normal play. See [Live Debug](<../../../../Editor User Guide/Run Debug and Package.md#live-debug>) for the editor controls.

## Native web pages

`Engine.OpenWebView(url)` opens an HTTP or HTTPS page above the game in a separate native layer. Its return value reports whether the request was accepted, not whether the network load succeeded; invalid URLs or an unavailable host return `false`. The Blueprint node uses ordinary execution pins and does not wait for the page to close.

- **Presentation and controls:** The overlay provides a close button, loading/error status and the interactive page (JavaScript, links, scrolling and text input). New-window links and repeated calls navigate the same layer. Downloads, extra device permissions and a JavaScript-to-game bridge are not exposed. Window replacement and application shutdown dismiss the layer. For input suppression, see [WebView input capture](<Input and Services.md#webview-input-capture>).
- **Platform implementations:** macOS and iOS use WKWebView, Android uses the system WebView, HarmonyOS uses ArkWeb, and Windows uses the installed Evergreen WebView2 Runtime (showing an installation link if missing; the game does not install it or bundle a browser engine).

## Typed data values

- `resolveTypedDataValue(value, type, environment?, declaringModule?, evaluateAnyExpressions?)` restores and validates stored values using the [Metadata Schema and Decorators](<../../../Blueprint Scripting/Metadata Schema and Decorators.md>) schema, including Union branch tags. `evaluateAnyExpressions` defaults to `true`; passing `false` keeps nested `any` strings as literals.
- `resolveRuntimeTypedValue(value, type, declaringModule?)` validates an existing runtime value and returns it without evaluating strings or reconstructing native objects.
- `setRuntimeTypedAttribute(owner, name, value)` validates and assigns an attribute using the owner's metadata. An explicit `nil` overrides inherited defaults on ordinary script fields.

## JSON data

JSON APIs operate strictly on the exact path in loose or packed `Data` and never select or decode an LDC alternative:

- `Engine.getJSONData(path)` parses JSON; `Engine.getJSONText(path)` reads the file as text; `Engine.jsonExists(path)` checks only that exact path. `Engine.writeJSON(path, value)` always writes JSON text regardless of extension.
- **Pure-data write boundary:** `Engine.writeJSON(filePath, value)` accepts registered JSON array markers (including empty arrays from Standard containers' `toTable()`) and `cjson.null`. Native objects, functions, cycles and unrecognised metatables are rejected; convert Standard containers explicitly before writing.
- **Lua read representation:** Arrays include a non-negative integer `n` recording logical length; null array slots are absent keys, whereas null object fields keep `cjson.null`. Array validators must account for `n`, and consumers must check required elements rather than treating `n` as an element.

## Data files

Use these functions for game resources and saves:

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `Engine.loadData` | `path: string` | `any` | Reads JSON or binary LDC. Prefers the exact path; only a missing `.json` path falls back to the matching `.ldc`. Explicit `.ldc` paths never fall back to JSON. |
| `Engine.saveData` | `path: string`, `value: any` | — | Writes JSON to `.json` or binary LDC to `.ldc`. Rejects other extensions and never renames the supplied path. |
| `Engine.dataExists` | `path: string` | `bool` | Checks existence using the same path resolution as `loadData`. |
| `Engine.decodeData` | `bytes: string` | `any` | Decodes complete JSON or LDC file bytes (including raw file-batch content). An `LDDC` header selects LDC exclusively; invalid LDC never falls back to JSON. |

These APIs share the JSON pure-data boundary and Lua array/null representation. LDC decodes directly into `RuntimeData` and then into Lua values without intermediate JSON text. Its payload is compressed and obfuscated; see [LDC binary data](<../../../../Native C++ Development/Build and Module Layout.md#ldc-binary-data>) for encoding and limits. Legacy LDC files containing compressed JSON are unsupported and must be regenerated from source data.

## SavePreviewReader

`Engine.SavePreviewReader` provides asynchronous save-slot scanning and thumbnail decoding. All public calls and polling must run on the game thread. This API is Lua-only and creates no Blueprint nodes.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `new` | — | `Engine.SavePreviewReader` | Static. Lua only. Creates an asynchronous save-preview reader. |
| `requestScan` | `paths: string[]` | — | Lua only. Scans up to 10,000 paths in the background; supersedes any prior scan. |
| `pollScan` | — | `bool` | Lua only. Returns `true` once per completed scan result. |
| `getLatestSlot` | — | `int` | Lua only. Returns the 1-based latest slot from the completed scan, or `0` if absent. |
| `getScanError` | — | `string` | Lua only. Returns the scan error string, or `""` on success. |
| `requestPreview` | `path: string`, `width: int`, `height: int` | — | Lua only. Requests metadata and a native thumbnail (each dimension in `1–1024`), reading the exact existing JSON or LDC file; supersedes any prior preview. |
| `pollPreview` | — | `bool` | Lua only. Returns `true` once per completed preview result. |
| `getState` | — | `string` | Lua only. Returns an `Engine.SavePreviewState` constant. A ready save without a screenshot has no image. |
| `getModificationTime` | — | `int` | Lua only. Returns the published Unix modification timestamp. |
| `getImage` | — | `sf.Image \| nil` | Lua only. Returns the decoded thumbnail image or `nil`. Create textures only on the game thread. |
| `getError` | — | `string` | Lua only. Returns the published preview error text. |
| `cancelPreview` | — | — | Lua only. Invalidates the pending preview without waiting for the worker. |
| `cancel` | — | — | Lua only. Invalidates both scan and preview without waiting for the worker. |
| `invalidate` | `path: string` | — | Static. Lua only. Invalidates a saved file's cached preview immediately after a successful write. |

`Engine.SavePreviewState` exposes read-only state strings: `IDLE = "idle"`, `LOADING = "loading"`, `EMPTY = "empty"`, `READY = "ready"` and `FAILED = "failed"`.

The native service caches up to eight thumbnails keyed by path, modification time, size and requested bounds. Cancel readers when their owner closes (destruction cancels automatically); engine shutdown discards queued work and joins the worker before releasing resource stores.
