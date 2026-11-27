# GlobalCore: Resource Managers

This page documents the static native resource managers under `GlobalCore` (`AudioManager`, `FontManager`, `ShaderManager` and `TextureManager`) and the Lua-only `TextureLoadRequest` handle. All manager methods are static and take no receiver; `TextureLoadRequest` methods are Lua-only instance methods.

All generic managers require `/Game/Assets/...` paths with exact case and read transparently from both loose files and `.ldpak` archives, with [user-directory resources taking priority](<../../../../Editor User Guide/Project Settings and Asset Layout.md#runtime-resource-lookup>). Native absolute paths, `Assets/...` paths and category-relative paths are invalid.

Archive compression is transparent: seekable streams decode one block at a time, report original file sizes and byte positions, and verify CRC32 per loaded block as well as on complete file reads.

## AudioManager

Controls audio playback, spatial references, filters, custom audio effects and persistent volume/mute switches for music, sound effects and voice. `set*` methods apply and save the value, whereas `save*` methods only persist it.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `loadSound` | `filePath: string` | `sf.SoundBuffer` | Static. |
| `playSound` | `filePath: string`, `filter: Engine.SoundFilter = nil`, `parent: sf.Transformable = nil` | `sf.Sound` | Static. |
| `setSoundParent` | `sound: sf.Sound`, `parent: sf.Transformable` | — | Static. |
| `playVoice` | `filePath: string`, `filter: Engine.SoundFilter = nil`, `refActor: sf.Transformable = nil`, `minDistance: float = 64` | `sf.Sound` | Static. |
| `setVoiceRefActor` | `voice: sf.Sound`, `refActor: sf.Transformable`, `minDistance: float = 64` | — | Static. |
| `playMusic` | `musicType: string`, `filePath: string`, `filter: Engine.MusicFilter = nil` | `sf.Music` | Static. |
| `stopSound` | — | — | Static. |
| `stopVoice` | — | — | Static. |
| `stopMusic` | `musicType: string` | — | Static. |
| `applySoundVolumes` | — | — | Static. |
| `applyVoiceVolumes` | — | — | Static. |
| `applyMusicVolumes` | — | — | Static. |
| `updateAllSoundPositions` | — | — | Static. |
| `updateAllVoicePositions` | — | — | Static. |
| `setSoundFilter` | `sound: sf.Sound`, `filter: Engine.SoundFilter` | — | Static. |
| `setVoiceFilter` | `voice: sf.Sound`, `filter: Engine.SoundFilter` | — | Static. |
| `setMusicFilter` | `music: sf.Music`, `filter: Engine.MusicFilter` | — | Static. |
| `setEffect` | `audioType: string`, `effect: function \| nil` | — | Static. Lua only (`metadata = false`). |
| `getMemory` | — | `int` | Static. |
| `getMusicOn` | — | `bool` | Static. |
| `setMusicOn` | `value: bool` | — | Static. |
| `saveMusicOn` | `value: bool` | — | Static. |
| `getSoundOn` | — | `bool` | Static. |
| `setSoundOn` | `value: bool` | — | Static. |
| `saveSoundOn` | `value: bool` | — | Static. |
| `getVoiceOn` | — | `bool` | Static. |
| `setVoiceOn` | `value: bool` | — | Static. |
| `saveVoiceOn` | `value: bool` | — | Static. |
| `getMusicVolume` | — | `float` | Static. |
| `setMusicVolume` | `value: float` | — | Static. |
| `saveMusicVolume` | `value: float` | — | Static. |
| `getSoundVolume` | — | `float` | Static. |
| `setSoundVolume` | `value: float` | — | Static. |
| `saveSoundVolume` | `value: float` | — | Static. |
| `getVoiceVolume` | — | `float` | Static. |
| `setVoiceVolume` | `value: float` | — | Static. |
| `saveVoiceVolume` | `value: float` | — | Static. |

`AudioManager.setEffect(audioType, effect)` accepts `"Sound"`, `"Voice"` or `"Music"` and registers a callback for future playback in that category (`nil` clears it; already-playing sources are unaffected):

- **Callback contract:** Signature is `fun(source: sf.SoundSource, control: GlobalCore.AudioEffectControl, sampleRate: integer)` with non-`nil` `source` and `control`. It runs once per new source before playback starts using the playback device's current sample rate, and must install exactly one non-empty processor (missing or duplicate installation fails synchronously).
- **Isolated Lua processor:** Call `control:attachLuaProcessor(source, moduleName, resolverName, effectName, sampleRate)` with three required project-supplied strings (the native layer defines no default module or resolver). Each attachment creates an isolated Lua state, loads `require(moduleName)`, calls `resolverName(effectName)`, and invokes the returned attacher as `(nil, control, sampleRate)` to produce an `sf.SoundSource.EffectProcessor`.
- **Control and errors:** The isolated control exposes `isCancelled()`, `beginTail()` and `finishTail()` and cannot attach another processor. Module loading, resolver and processor-creation errors propagate synchronously without falling back to unprocessed audio; later audio-callback errors propagate during Manager updates.
- **Search paths and isolation:** The isolated state inherits the module search path captured when `AudioManager` initialises, so configure extra search paths before loading `GlobalCore`. Main-state closures and objects do not transfer to the audio state. See [default gameplay audio](<../../../Default Gameplay/Resources Audio Video and Effects.md#audio>) for supplied presets, playback lifetime and cancellation rules.

## FontManager

Loads and queries cached `sf.Font` resources.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `load` | `filePath: string` | `sf.Font` | Static. |
| `getFont` | `fontName: string` | `sf.Font` | Static. |
| `getFontFilename` | `fontName: string` | `string` | Static. |
| `getFontList` | — | `string[]` | Static. |
| `getFontFilenameList` | — | `string[]` | Static. |
| `hasFont` | `fontName: string` | `bool` | Static. |
| `getMemory` | — | `int` | Static. |

## ShaderManager

Loads pre-prepared shader programs. Finish [shader preparation](<../Engine/Maps and Materials.md#shader-preparation-in-mrb>) before calling these methods, and register any non-default stage combinations in that request; load methods do not translate or compile missing programs during rendering.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `load` | `shaderPath: string`, `shaderType: sf.Shader.Type = nil` | `sf.Shader` | Static. |
| `loadFull` | `vertPath: string`, `fragPath: string` | `sf.Shader` | Static. |
| `loadFullShaderWithGeo` | `vertPath: string`, `geoPath: string`, `fragPath: string` | `sf.Shader` | Static. |
| `getMemory` | — | `int` | Static. |

## TextureManager

Loads textures synchronously (`load`) or asynchronously (`request`). `request` returns a `TextureLoadRequest` immediately while file reading, archive decompression and image decoding run on background workers; the running System frame loop uploads decoded images to the GPU before publishing Ready.

> **Important:** Requesting a texture does not make it drawable immediately. Poll the handle from later updates rather than spinning in a loop that blocks the frame.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `load` | `filePath: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | Static. |
| `request` | `filePath: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false`, `priority: int = 0` | `GlobalCore.TextureLoadRequest` | Static. Lua only (`metadata = false`). |
| `getPath` | `texture: sf.Texture` | `string \| nil` | Static. Lua only (`metadata = false`). |
| `getMemory` | — | `int` | Static. |

Cache and lookup rules:

- **`getPath(texture)`:** Finds the exact texture instance in the current cache and returns its original resource path without reading files or retaining the texture. Textures created outside the manager or evicted by cache clearing return `nil`. The path does not encode `area`, `sRGB` or `smooth` load options.
- **Cache sharing:** `load` and `request` share the same cache and in-flight work. Exact path, `sRGB`, `area` and `smooth` form the cache key (priority does not). Repeated requests for the same entry share decoding and upload while returning independent subscription handles.
- **Ready and retry semantics:** Cache hits start Ready immediately. Synchronous `load` can complete a matching pending request without waiting for another frame, allowing use before the System frame loop begins. Failed results are not cached, so subsequent requests retry the load.

### TextureLoadRequest

Instance handle returned by `TextureManager.request`. It retains its ready texture until cancelled or released without invalidating other holders. The manager keeps weak texture references on ordinary platforms and strong references on iOS until the cache is cleared.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getStatus` | — | `string` | Lua only. Returns read-only constants `TextureLoadRequest.PENDING` (`"pending"`), `READY` (`"ready"`), `FAILED` (`"failed"`) or `CANCELLED` (`"cancelled"`). Never waits or raises stored load errors. |
| `getTexture` | — | `sf.Texture` | Lua only. Returns the ready texture, raises the stored load error on Failed, or raises a state error on Pending/Cancelled. Never waits. |
| `resolve` | — | `sf.Texture` | Lua only. Finishes the request synchronously on the calling thread and returns the texture. Raises the stored error on Failed or a state error on Cancelled. |
| `setPriority` | `priority: int` | — | Lua only. Updates this pending subscription's integer priority (larger runs first; request order breaks ties). Completed, failed and cancelled handles are unchanged. |
| `cancel` | — | — | Lua only. Releases this subscription and marks the handle Cancelled (including after Ready or Failed). Safe to repeat; textures already obtained stay valid. |

These methods and constants are Lua-only and do not create Blueprint nodes. A `TextureLoadRequest` is not an `Engine.AsyncOperation` and cannot be passed to `asyncio.await`. Poll its state from normal updates and call `getTexture()` on Ready or Failed so required-resource errors propagate.

Scheduling, cancellation and budget behaviour:

- **Shared subscriptions:** The highest priority among pending subscriptions governs shared work. Cancelling or releasing one handle leaves other subscribers active; when the last subscriber leaves, queued work cancels and in-flight results are discarded (already-running decodes or uploads are not interrupted). Runtime shutdown cancels outstanding requests and joins decoding workers before closing the graphics context.
- **Worker and backlog limits:** Decoding workers start on demand up to the [process's file-read limit](<../../../../Getting Started/Ludork Lua Advanced.md#cooperative-tasks-and-file-batches>). A 64 MiB decoded-pixel backlog pauses new background decodes while in-flight decodes finish. The backlog counts full decoded images even when `area` selects a smaller GPU texture.
- **Per-frame upload budgets:** Each frame starts at most two GPU uploads within an 8 MiB byte budget and a 1 ms time budget (a first texture exceeding 8 MiB is still permitted to proceed, and started uploads are not interrupted). These are scheduling limits rather than hard frame-time or total-memory caps; decoding can temporarily hold extra pixel copies and codec buffers. Uploads pause while a mobile app is suspended.
