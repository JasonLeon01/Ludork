# GlobalCore: Scenes and System

This page documents the `SceneBase` lifecycle and timer API together with the static runtime services under `GlobalCore`: `System`, `Display`, `Graphics`, `ScreenEffects`, `Transition`, `SceneManager` and `RuntimeDiagnostics`. `SceneBase` method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; all runtime services below `SceneBase` are static types whose methods take no receiver.

## SceneBase

Base class for gameplay scenes, managing per-frame callbacks, UI managers, animations and timers.

- **Threading model:** On HarmonyOS, the game thread runs `onTick`, advances due `onFixedTick` steps and renders the frame. WebGL runs the same callbacks and rendering on the browser main thread. Other native hosts run scene logic on a dedicated logic worker while the game thread renders.
- **Window close:** Closing the window during runtime or input processing (including from an input or UI callback) ends the current frame before further UI updates, emitter simulation or rendering, stops scene logic and runs `onQuit` through the normal scene exit lifecycle.
- **Focus loss and suspend:** On Android, iOS, HarmonyOS and WebGL, losing focus (or hiding a Web page) pauses scene and UI updates, rendering, runtime callbacks and scene timers while system event processing continues. Returning to the foreground in the same process preserves the scene and Lua state, resets frame and fixed-update clocks, and resumes without catching up on background time. Closing follows the normal exit lifecycle.

### Scene timers

`scene:addTimer(interval, task, params, blocking)` returns an `Engine.AsyncOperation`. On expiry, it runs `task` before completing with `true` and resuming the Blueprint `TimeUp` branch.

If the callback raises a Lua runtime error, the engine logs the Lua stack trace, cancels the operation and releases its input block; waiters follow the [operation cancellation rules](<../Engine/Events and Module Root.md#asyncoperation>), and other timers due in the same update continue.

The Lua-only `scene:cancelTimers()` cancels unfinished timers and clears their input blocks. Calling it from a timer callback also cancels the current operation and skips remaining callbacks. Scene exit or destruction cancels timers automatically.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getUIManager` | — | `GlobalCore.UIManager` | Pure. |
| `addTimer` | `interval: float = nil`, `task: function = nil`, `params: any[] = {}`, `blocking: bool = false` | `Engine.AsyncOperation` | Latent. Branch: `TimeUp` (`true`). |
| `isInputBlocked` | — | `blocked: bool` | Pure. |
| `addAnim` | `anim: GlobalCore.Animation` | — | Exec (`default`). |
| `getAnims` | — | `anims: GlobalCore.Animation[]` | Pure. |
| `removeAnim` | `anim: GlobalCore.Animation` | — | Exec (`default`). |
| `clearAnims` | — | — | Exec (`default`). |
| `addCommonTip` | `text: string` | — | Exec (`default`). |
| `onEnter` | — | — | — |
| `onQuit` | — | — | — |
| `onCreate` | — | — | — |
| `onInput` | — | — | — |
| `onTick` | `deltaTime: float` | — | — |
| `onLateTick` | `deltaTime: float` | — | — |
| `onFixedTick` | `fixedDelta: float` | — | — |
| `onDestroy` | — | — | — |

All runtime services below are static types under `GlobalCore`. The [game configuration](<../../../Default Gameplay/Runtime Data Configuration and Saves.md#configuration>) uses the same keys and defaults on every platform; each service owns its values and apply/save behaviour.

## System

`GlobalCore.System` owns configuration startup, the main loop, runtime lifecycle and language settings. `System.run()` enters the loop from Lua and is excluded from Blueprint metadata.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `init` | `data: ludork::standard::ConfigParser`, `dataFilePath: string` | — | Static. |
| `getLanguage` | — | `string` | Static. |
| `setLanguage` | `value: string` | — | Static. |
| `saveLanguage` | `value: string` | — | Static. |
| `isDebugMode` | — | `bool` | Static. Pure. |
| `exit` | — | — | Static. |

## Display

`GlobalCore.Display` owns the window, logical size, display scale, input method and presentation settings. `Display.initializeDisplay(title, gameSize, iconPath, cursorPath)` prepares the display from Lua and is excluded from Blueprint metadata.

### Display scale semantics

- **Configured vs effective scale:** `getConfiguredScale()` returns the configured preference (including `0` for Fullscreen, the default). `getScale()` returns the positive effective scale used by rendering and input after Maximum Render Scale is applied. Non-finite or negative values normalise to `1`.
- **Apply and save methods:** `setScale` applies and saves the value; `applyScale` only applies it; `saveScale` only saves it.
- **Desktop windows:** On desktop, `0` selects borderless fullscreen without changing display mode, while positive values set a fixed standalone window size (border drag resizing is disabled) with aspect-ratio-preserving centred black bars when needed.
- **macOS backing scale:** Configured window scale is measured in Cocoa client points, whereas the effective render scale returned by `getScale()` and `Engine.GetScale()` derives from actual drawable pixels (including Retina backing scale) capped by Maximum Render Scale. Backing-scale changes rebuild render targets and refresh text without resizing the logical window.
- **Configurability (`isDisplayScaleConfigurable()`):** Returns `true` for standalone desktop windows (including HarmonyOS 2-in-1) and HarmonyOS tablets in free-window mode, and `false` for embedded hosts, ordinary mobile devices and WebGL.
- **WebGL:** Page CSS and browser device-pixel ratio (DPR) control the canvas size; configured scale (including `0`) never resizes the canvas or requests fullscreen, while Maximum Render Scale caps internal render resolution.
- **HarmonyOS 2-in-1 and free-window tablets:** HarmonyOS 2-in-1 follows the desktop protocol (default `0`). On HarmonyOS phones and tablets, the ArkTS host listens to `freeWindowModeChange` and reapplies configured scale when free-window mode turns on or returns to the foreground: positive values request a `round(gameSize * scale)` pixel client area (adding decorations for outer Stage size), `0` requests maximised or fullscreen, drag resizing stays disabled, and rendering letterboxes with centred black bars if system limits apply.
- **Maximum windowed scale (`getMaximumWindowedScale(gameSize)`):** Returns the largest scale fitting the available window dimensions — the current window's decorated work area (or primary display when no window exists) on desktop and HarmonyOS 2-in-1, or a host-supplied maximum on configurable mobile hosts (refreshed when HarmonyOS free-window mode is enabled or resumed). Returns `nil` for embedded hosts, non-configurable mobile hosts, zero logical dimensions or unknown available sizes.

### Anti-aliasing semantics

Anti-aliasing defaults to `8`; an invalid value falls back to the SFML default. `setAntiAliasingLevel` saves the value without recreating the current context, taking effect at the next display initialisation. SFML may negotiate another supported level.

### Frame rate semantics

`getFrameRate()` returns the configured target (default `60`). `0` means Unlimited and disables active frame-rate limiting, though Vertical Sync remains independent and can still cap presentation. `setFrameRate` applies and saves; `saveFrameRate` only saves. Fixed updates use a `1/60`-second step when Unlimited, or `1/framerate` for a positive target.

### iOS graphics context

iOS startup selects Metal; see [MRB rendering](<../../../../Native C++ Development/Build and Module Layout.md#mrb-rendering>).

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getScale` | — | `float` | Static. |
| `getConfiguredScale` | — | `float` | Static. |
| `getMaximumWindowedScale` | `gameSize: sf.Vector2u` | `float \| nil` | Static. Pure. |
| `setScale` | `value: float` | — | Static. |
| `applyScale` | `value: float` | — | Static. |
| `saveScale` | `value: float` | — | Static. |
| `isDisplayScaleConfigurable` | — | `bool` | Static. Pure. |
| `getFrameRate` | — | `int` | Static. |
| `setFrameRate` | `value: int` | — | Static. |
| `saveFrameRate` | `value: int` | — | Static. |
| `getAntiAliasingLevel` | — | `int` | Static. |
| `setAntiAliasingLevel` | `value: int` | — | Static. |
| `saveAntiAliasingLevel` | `value: int` | — | Static. |
| `getVerticalSync` | — | `bool` | Static. |
| `setVerticalSync` | `value: bool` | — | Static. |
| `saveVerticalSync` | `value: bool` | — | Static. |
| `getGameSize` | — | `sf.Vector2u` | Static. Pure. |
| `setInputMethodDisabled` | `disabled: bool` | — | Static. |

## Graphics

`GlobalCore.Graphics` owns the render canvas, views, drawing, ordered full-screen shader chain and frame submission.

> **Important:** Graphics canvas textures use premultiplied alpha. Custom full-screen shaders must output premultiplied or opaque colours.

### Maximum render scale semantics

`getMaximumRenderScale()` returns the cap on the effective physical-pixel render scale: `0` is unlimited, the default is `2`, and invalid values fall back to `2`. `setMaximumRenderScale` applies between frames without resizing the window and saves; `saveMaximumRenderScale` only saves.

For example, for a `640×480` game at configured scale `1.5` on a Retina 2× display, the window is `960×720` Cocoa points: an unlimited cap renders at `1920×1440` (effective scale `3`), whereas a cap of `2` renders at `1280×960` in the same window.

### Lighting render scale semantics

`getLightingRenderScale()` returns `0.5`, `0.75` or `1` (default `1`, also used for invalid values). `setLightingRenderScale` applies and saves, rebuilding direct-light targets and invalidating the static cache; `saveLightingRenderScale` only saves. The surface mask, transmission and occupancy stay at full resolution.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getMaximumRenderScale` | — | `float` | Static. |
| `setMaximumRenderScale` | `value: float` | — | Static. |
| `saveMaximumRenderScale` | `value: float` | — | Static. |
| `getLightingRenderScale` | — | `float` | Static. |
| `setLightingRenderScale` | `value: float` | — | Static. |
| `saveLightingRenderScale` | `value: float` | — | Static. |
| `setWindowMapView` | `rect: sf.IntRect` | — | Static. |
| `setWindowDefaultView` | — | — | Static. |
| `getCanvas` | — | `sf.RenderTexture` | Static. Pure. |
| `draw` | `drawable: sf.Drawable`, `shader: sf.Shader = nil` | — | Static. |
| `addGraphicsShader` | `shader: sf.Shader`, `uniforms: any = nil` | — | Static. |
| `removeGraphicsShader` | `shader: sf.Shader` | — | Static. |
| `removeAllGraphicsShaders` | — | — | Static. |
| `removeGraphicsShaderAt` | `index: int` | — | Static. |

## ScreenEffects

`GlobalCore.ScreenEffects` owns screen flash, screen shake and screen tone effects along with their active and completion queries.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `flashScreen` | `color: sf.Color = nil`, `duration: float = 0.5` | — | Static. |
| `stopFlash` | — | — | Static. |
| `isFlashing` | — | `bool` | Static. Pure. |
| `changeScreenTone` | `red: float = 0`, `green: float = 0`, `blue: float = 0`, `gray: float = 0`, `duration: float = 0` | — | Static. |
| `clearScreenTone` | `duration: float = 0` | — | Static. |
| `stopScreenTone` | — | — | Static. |
| `isScreenToneActive` | — | `bool` | Static. Pure. |
| `startShake` | `power: float = 4`, `speed: float = 10`, `duration: float = 0.5` | — | Static. |
| `stopShake` | — | — | Static. |
| `isShaking` | — | `bool` | Static. Pure. |
| `isScreenToneTransitionComplete` | — | `bool` | Static. Pure. |

## Transition

`GlobalCore.Transition` owns transition requests and frozen backgrounds. `requestTransition` accepts an empty optional value or a complete `/Game/Assets/Transitions/...` logical path without adding a prefix.

- **Completion timing:** `setTransition`, `freezeTransitionBackground` and `requestTransition` return an `Engine.AsyncOperation` completing with `true`. Freeze requests complete once the outgoing frame is captured; transitions complete after the final frame is submitted. For Blueprint `Frozen` / `Finished` branches, use the [Transition nodes](<../../../Node Functions/Transition.md>).
- **Cancellation:** Starting a new request of the same kind cancels any previous unfinished operation. `cancelTransitionBackgroundFreeze()` cancels the freeze operation and clears the frozen state, whereas `cancelPendingTransition()` cancels only an unapplied request without stopping a running transition. Runtime reset or shutdown cancels remaining operations.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setTransition` | `transitionResource: sf.Texture = nil`, `transitionTime: float = 1` | `Engine.AsyncOperation` | Static. |
| `freezeTransitionBackground` | — | `Engine.AsyncOperation` | Static. |
| `isTransitionBackgroundFrozen` | — | `bool` | Static. Pure. |
| `isTransitionBackgroundFreezePending` | — | `bool` | Static. Pure. |
| `cancelTransitionBackgroundFreeze` | — | — | Static. |
| `requestTransition` | `transitionName: string = nil`, `transitionTime: float = 1` | `Engine.AsyncOperation` | Static. |
| `cancelPendingTransition` | — | — | Static. |
| `isTransitionPending` | — | `bool` | Static. Pure. |
| `isInTransition` | — | `bool` | Static. Pure. |

## SceneManager

`GlobalCore.SceneManager` owns the current scene, scene stack and queued scene transitions, deferring destruction of outgoing scenes. Request application exit through `System.exit()`.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getScene` | — | `SceneRuntime` | Static. Pure. |
| `requireScene` | — | `SceneRuntime` | Static. Pure. |
| `getSceneList` | — | `SceneRuntime[]` | Static. Pure. |
| `setScene` | `scene: SceneRuntime` | — | Static. |
| `pushScene` | `scene: SceneRuntime` | — | Static. |
| `popScene` | — | — | Static. |

## RuntimeDiagnostics

Exposes Lua-only profiling queries and sample recorders (`metadata = false`, no Blueprint nodes):

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `isPerformanceProfilerEnabled` | — | `bool` | Static. Lua only (`metadata = false`). |
| `recordWorldStreamingPerformance` | `queueDepth: int`, `reading: int`, `prepared: int`, `active: int`, `dormant: int`, `cacheBytes: int`, `publishMilliseconds: float`, `visibleTileChunks: int`, `activeActors: int` | — | Static. Lua only (`metadata = false`). |

Native performance output also records `textureUpload` (frame upload duration in milliseconds, including graphics-context activation) and a `textureStreaming` object in each `mainFrames` entry:

| Field | Description |
|---|---|
| `cacheHits`, `mergedRequests` | Session totals for requests served from ready textures and requests joined to in-flight work. |
| `decodedCount`, `uploadedCount`, `failedCount` | Session totals for CPU decodes, GPU uploads and decode/upload failures. Shared work is counted once per load rather than per subscriber; started work can still increment these after cancellation. |
| `cancelledCount` | Pending subscriptions cancelled during the session. Releasing a handle after GPU upload does not increment this count. |
| `decodeMilliseconds`, `uploadMilliseconds`, `uploadedBytes` | Cumulative reading/decoding time (ms), GPU upload time (ms) and estimated uploaded bytes. |
| `pendingRequests`, `decodingRequests`, `readyRequests` | Current counts of queued, decoding and decoded-awaiting-upload work items. |
| `readyBytes`, `peakReadyBytes` | Current and peak decoded-pixel backlog in bytes. Excludes decoder scratch memory and GPU allocations, so this is not total memory usage. |

For how the decoded backlog and per-frame upload budgets govern resource readiness, see [TextureManager scheduling limits](<Resource Managers.md#textureloadrequest>).

For related services: weather and panorama live in `WeatherController` and `PanoramaController`, per-map fog lives on each `GameMap` (`getFogSettings()` / `setFogSettings(settings)` — see [Map and Fog](<Map and Fog.md>)), and audio switches and volumes belong to [AudioManager](<Resource Managers.md#audiomanager>).
