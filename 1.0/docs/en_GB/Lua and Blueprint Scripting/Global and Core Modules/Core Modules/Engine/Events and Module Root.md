# Engine: Events and Module Root

## AsyncOperation

`Engine.AsyncOperation` represents asynchronous work shared by Lua and Blueprint. Lua callers wait with `asyncio.await(operation)` inside a task; Blueprint latent nodes resume through their declared stages.

| API | Contract |
|---|---|
| `AsyncOperation.new(poll?)` | Create a pending operation. Optional `poll(operation)` runs once per logic update until completion or cancellation. |
| `operation:emit(value)` | Publish an intermediate Blueprint stage without completing Lua waiters. |
| `operation:complete(result?)` | Complete with one retained result, also used as the final Blueprint stage value. `nil`, `false` and `0` are valid results. |
| `operation:cancel()` | Cancel the operation and its waiting executions. Completed or cancelled operations remain unchanged. |
| `operation:getStatus()` | Return the read-only string constants `AsyncOperation.PENDING` (`"pending"`), `COMPLETED` (`"completed"`) or `CANCELLED` (`"cancelled"`). |
| `operation:getResult()` | Read the completed result; pending or cancelled operations raise an error. |

Multiple waiters can share an operation without consuming one another's stages. A completed operation can be awaited repeatedly; new Blueprint waiters receive only its final result. Poll callbacks run on the logic thread, must not yield, and propagate errors to the host.

The operation's owner handles resource cleanup; cancellation alone does not undo business side effects. See [Lua task usage and cancellation](<../../../../Getting Started/Ludork Lua Advanced.md#cooperative-tasks-and-file-batches>) and [Blueprint cancellation](<../../../Blueprint Scripting/Execution Flow Events and Variables.md#cancelling-latent-execution>).

## EventBus

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `subscribe` | `function` | self: { "Engine", "EventBus" } = "self"; event: string; handler: function; priority: int = 0 | return: int | — |
| `once` | `function` | self: { "Engine", "EventBus" } = "self"; event: string; handler: function; priority: int = 0 | return: int | — |
| `subscribeObjectHandler` | `function` | self: { "Engine", "EventBus" } = "self"; event: string; object: any; handler: function; priority: int = 0 | return: int | — |
| `subscribeBlueprintEvent` | `function` | self: { "Engine", "EventBus" } = "self"; event: string; object: any; eventName: string; priority: int = 0 | return: int | — |
| `onceBlueprintEvent` | `function` | self: { "Engine", "EventBus" } = "self"; event: string; object: any; eventName: string; priority: int = 0 | return: int | — |
| `unsubscribe` | `function` | self: { "Engine", "EventBus" } = "self"; token: int | return: bool | — |
| `unsubscribeEvent` | `function` | self: { "Engine", "EventBus" } = "self"; event: string | return: bool | — |
| `unsubscribeObjectHandler` | `function` | self: { "Engine", "EventBus" } = "self"; event: string; object: any | return: bool | — |
| `clear` | `function` | self: { "Engine", "EventBus" } = "self"; event: string = nil | — | — |
| `publish` | `function` | self: { "Engine", "EventBus" } = "self"; event: string; payload: any = nil | — | — |
| `post` | `function` | self: { "Engine", "EventBus" } = "self"; event: string; payload: any = nil | — | — |
| `flush` | `function` | self: { "Engine", "EventBus" } = "self"; limit: int = nil | return: int | — |

## Engine

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `ZeroVector2f` | `sf.Vector2f` | — | — |
| `ZeroVector2i` | `sf.Vector2i` | — | — |
| `ZeroVector2u` | `sf.Vector2u` | — | — |
| `ZeroVector3f` | `sf.Vector3f` | — | — |
| `ZeroVector3i` | `sf.Vector3i` | — | — |
| `ZeroVector3u` | `sf.Vector3u` | — | — |
| `Direction` | enum table | — | — |
| `FocusDirection` | `any` | — | — |
| `DefaultFontSize` | `int` | — | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `GetCellSize` | `function` | — | return: int | Pure |
| `GetGameSize` | `function` | — | return: sf.Vector2u | Pure |
| `GetScale` | `function` | — | return: float | Pure |
| `IsGameRunning` | `function` | — | return: bool | Pure |
| `OpenWebView` | `function` | url: string | accepted: bool | Ordinary execution; returns when the request is accepted |
| `C_CompressAnimation` | `function` | frameCount: int; frameStep: float; frameRate: int; timeLines: { "Engine", "AnimationTimeline[]" }; assets: string[]; imageFormat: string | duration: float; frames: string[]; sounds: { "Engine", "AnimationSoundEntry[]" } | — |

`Engine.GetCellSize()` returns the C++ compile-time cell size, currently `32`, and has no runtime setter. `GetGameSize()` returns a detached logical-size value, `GetScale()` returns the effective render scale, and `IsGameRunning()` reads lifecycle state. Use `GlobalCore.Display` display commands and `GlobalCore.System.exit()` to change runtime state. These queries do not expose writable state.

`Engine.Direction` and `require("Enums.Engine.Direction")` expose the same native enum values: `DOWN = 0`, `LEFT = 1`, `RIGHT = 2`, `UP = 3`. Use the named members for Actor facing and `Engine.OppositeDirection(direction)` to obtain the opposite direction.

`Engine.DisplayScaleLimits` provides read-only numeric constants. `MINIMUM_RENDER_SCALE` (`0.01`) is the effective render-scale lower bound; `MINIMUM_INVERTIBLE_SCALE` (`0.000001`) is the guard used before dividing by a display scale. Each constant describes its own limit.

## ResourceFileConstants

`Engine.ResourceFileConstants` exposes static, read-only string properties shared by native resource loaders, Lua and packaging tools. The class uses `metadata = false` and does not add Blueprint nodes.

| Property | Type | Value |
|---|---|---|
| `LUA_SOURCE_EXTENSION` | `string` | `".lua"` |
| `LUA_COMPILED_EXTENSION` | `string` | `".luac"` |
| `DATA_EXTENSION` | `string` | `".json"` |
| `ENCRYPTED_DATA_EXTENSION` | `string` | `".ldc"` |
| `ANIMATION_CACHE_SUFFIX` | `string` | `".anim.json"` |
| `ENCRYPTED_ANIMATION_CACHE_SUFFIX` | `string` | `".anim.ldc"` |

## EditorLiveDebug

`Engine.EditorLiveDebug` is a built-in editor service. The class and both static methods use `metadata = false`: they are available to Lua and generated declarations without exposing Blueprint nodes.

| Method | Parameters | Result |
|---|---|---|
| `install` | handler: function | Installs or replaces the current VM's request handler. |
| `uninstall` | — | Releases that handler; repeated calls are harmless. |

The handler receives one request table and must return exactly one JSON-serialisable table. Missing handlers, Lua errors, invalid return counts and encoding failures produce `success = false` with an `error` string. The default Game module owns registration; project scripts should not replace the handler during normal play. Installation does nothing without an editor bridge.

Registration belongs to the Lua VM and is released at shutdown. See [Live Debug](<../../../../Editor User Guide/Run Debug and Package.md#live-debug>) for the editor controls.

## Native web pages

`Engine.OpenWebView(url)` opens an HTTP or HTTPS page above the game in a separate native layer. The return value indicates whether the request was accepted, not whether the network load succeeded. Invalid addresses or an unavailable host return `false`. The Blueprint node has ordinary execution pins and a boolean return; it does not wait for the page to close.

The layer has a close button, a loading/error status and the interactive page. JavaScript, links, scrolling and text input work inside the system WebView. New-window links and repeated calls navigate the same layer. Downloads, extra device permissions and a JavaScript-to-game bridge are not exposed. Input capture is described in [Input and Services](<Input and Services.md#webview-input-capture>). Window replacement and application shutdown dismiss the page.

macOS and iOS use WKWebView, Android the system WebView, HarmonyOS ArkWeb, and Windows the installed Evergreen WebView2 Runtime. Windows shows an installation link when that Runtime is missing; the game does not install it or bundle a browser engine.

## Typed data values

`resolveTypedDataValue(value, type, environment?, declaringModule?, evaluateAnyExpressions?)` restores and validates stored values using the [Metadata Schema and Decorators](<../../../Blueprint Scripting/Metadata Schema and Decorators.md>) schema, including Union branch tags. `evaluateAnyExpressions` defaults to `true`, and passing `false` preserves nested `any` strings as literals.

`resolveRuntimeTypedValue(value, type, declaringModule?)` validates an existing runtime value and returns it without evaluating strings or reconstructing native objects.

`setRuntimeTypedAttribute(owner, name, value)` validates and assigns an attribute using the owner's metadata. An explicit nil overrides inherited defaults on ordinary script fields.

## JSON data

JSON APIs address the exact path in loose or packed Data and never select or decode an LDC alternative. `Engine.getJSONData(path)` parses JSON; `Engine.getJSONText(path)` reads the file as text; `Engine.jsonExists(path)` checks only that path. `Engine.writeJSON(path, value)` always writes JSON text, regardless of the filename extension.

`Engine.writeJSON(filePath, value)` and `Engine.getJSONData(filePath)` use the native pure-data boundary. Writing accepts registered JSON array markers, including empty arrays from Standard containers' `toTable()`, and `cjson.null`. Native objects, functions, cycles and unrecognised metatables are rejected. Convert Standard containers explicitly before writing.

Reading returns ordinary Lua values. Arrays include a non-negative integer `n` for their logical length; null array slots are absent keys, while null object fields retain `cjson.null`. Array validators must account for `n`, and consumers must check each required element rather than treating length metadata as an element.

## Data files

Use these APIs for game resources and saves:

| API | Contract |
|---|---|
| `Engine.loadData(path)` | Read JSON or binary LDC. Prefer the exact path; only a missing `.json` path resolves to the matching `.ldc`. Explicit `.ldc` paths never resolve back to JSON. |
| `Engine.saveData(path, value)` | Write JSON to `.json` or binary LDC to `.ldc`. Reject other extensions and never rename the supplied path. |
| `Engine.dataExists(path)` | Check existence using the same path resolution as `loadData`. |
| `Engine.decodeData(bytes)` | Decode complete JSON or LDC file bytes, including raw file-batch content. An `LDDC` header selects LDC exclusively; invalid LDC never falls back to JSON. |

These APIs use the same pure-data boundary and Lua array/null representation as the JSON APIs. LDC is decoded directly into `RuntimeData`, then into Lua values, without generating or parsing intermediate JSON text. Its payload is compressed and obfuscated; see the [native format contract](<../../../../Native C++ Development/Build and Module Layout.md#ldc-binary-data>) for encoding and limits. Old LDC files containing compressed JSON are unsupported and must be regenerated from their source data.

## SavePreviewReader

`Engine.SavePreviewReader.new()` creates an asynchronous save-preview reader. All public calls and polling belong to the game thread. This Lua-only API has no Blueprint nodes.

| Method | Contract |
|---|---|
| `requestScan(paths)` / `pollScan()` | Scan up to 10,000 paths in the background; polling returns true once per completed result. |
| `getLatestSlot()` / `getScanError()` | Read the completed scan: one-based latest slot, zero if absent, and an empty string on success. |
| `requestPreview(path, width, height)` / `pollPreview()` | Request metadata and a native thumbnail, with each dimension in 1–1024; polling publishes a completed result. The exact existing file is read as JSON or LDC. |
| `getState()` | Return an `Engine.SavePreviewState` string value. A ready save without a screenshot has no image. |
| `getModificationTime()` / `getImage()` / `getError()` | Read the published Unix timestamp, `sf.Image` or nil, and error text. Create textures only on the game thread. |
| `cancelPreview()` / `cancel()` | Invalidate the pending preview, or both scan and preview, without waiting for the worker. New requests also supersede older results of the same kind. |
| `SavePreviewReader.invalidate(path)` | Invalidate a saved file's cached preview immediately after a successful write. |

`Engine.SavePreviewState` exposes read-only strings: `IDLE = "idle"`, `LOADING = "loading"`, `EMPTY = "empty"`, `READY = "ready"` and `FAILED = "failed"`. Use these members when checking a reader state.

The native service retains up to eight thumbnails by path, modification time and size. Requested bounds also participate in cache matching. Readers must be cancelled when their owner closes; destruction cancels automatically. Engine shutdown discards queued work and joins the worker before resource stores are released.
