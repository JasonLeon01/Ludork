# GlobalCore: Resource Managers

The generic managers require `/Game/Assets/...` paths with exact case, and they read both loose files and `.ldpak` archives. Native absolute paths, `Assets/...` paths and category-relative paths are invalid.

Archive compression is transparent to these APIs. Seekable streams decode one block at a time and report original file sizes and byte positions. Each loaded block is CRC32-checked; complete reads also check the file CRC32.

## AudioManager

Music, sound and voice switches and volumes are configured here. `set*` applies and saves, while `save*` only persists the value.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `loadSound` | `function` | filePath: string | return: sf.SoundBuffer | — |
| `playSound` | `function` | filePath: string; filter: { "Engine", "SoundFilter" } = nil; parent: sf.Transformable = nil | return: sf.Sound | — |
| `setSoundParent` | `function` | sound: sf.Sound; parent: sf.Transformable | — | — |
| `playVoice` | `function` | filePath: string; filter: { "Engine", "SoundFilter" } = nil; refActor: sf.Transformable = nil; minDistance: float = 64 | return: sf.Sound | — |
| `setVoiceRefActor` | `function` | voice: sf.Sound; refActor: sf.Transformable; minDistance: float = 64 | — | — |
| `playMusic` | `function` | musicType: string; filePath: string; filter: { "Engine", "MusicFilter" } = nil | return: sf.Music | — |
| `stopSound` | `function` | — | — | — |
| `stopVoice` | `function` | — | — | — |
| `stopMusic` | `function` | musicType: string | — | — |
| `applySoundVolumes` | `function` | — | — | — |
| `applyVoiceVolumes` | `function` | — | — | — |
| `applyMusicVolumes` | `function` | — | — | — |
| `updateAllSoundPositions` | `function` | — | — | — |
| `updateAllVoicePositions` | `function` | — | — | — |
| `setSoundFilter` | `function` | sound: sf.Sound; filter: { "Engine", "SoundFilter" } | — | — |
| `setVoiceFilter` | `function` | voice: sf.Sound; filter: { "Engine", "SoundFilter" } | — | — |
| `setMusicFilter` | `function` | music: sf.Music; filter: { "Engine", "MusicFilter" } | — | — |
| `setEffect` | `function` | audioType: string; effect: callback or nil | — | Lua only; `metadata = false` |
| `getMemory` | `function` | — | return: int | — |
| `getMusicOn` | `function` | — | return: bool | — |
| `setMusicOn` | `function` | value: bool | — | — |
| `saveMusicOn` | `function` | value: bool | — | — |
| `getSoundOn` | `function` | — | return: bool | — |
| `setSoundOn` | `function` | value: bool | — | — |
| `saveSoundOn` | `function` | value: bool | — | — |
| `getVoiceOn` | `function` | — | return: bool | — |
| `setVoiceOn` | `function` | value: bool | — | — |
| `saveVoiceOn` | `function` | value: bool | — | — |
| `getMusicVolume` | `function` | — | return: float | — |
| `setMusicVolume` | `function` | value: float | — | — |
| `saveMusicVolume` | `function` | value: float | — | — |
| `getSoundVolume` | `function` | — | return: float | — |
| `setSoundVolume` | `function` | value: float | — | — |
| `saveSoundVolume` | `function` | value: float | — | — |
| `getVoiceVolume` | `function` | — | return: float | — |
| `setVoiceVolume` | `function` | value: float | — | — |
| `saveVoiceVolume` | `function` | value: float | — | — |

`AudioManager.setEffect(audioType, effect)` accepts `Sound`, `Voice` or `Music` and stores a callback for future playback in that category; `nil` clears it. It does not change a source already playing. The callback has the signature `fun(source: sf.SoundSource, control: GlobalCore.AudioEffectControl, sampleRate: integer)`, with non-nil source and control arguments. Each new source invokes it once before playback, using the current playback-device sample rate. It must install exactly one non-empty processor; missing or duplicate installation fails synchronously.

For a Lua processor, call `control:attachLuaProcessor(source, moduleName, resolverName, effectName, sampleRate)`. The three names are required strings supplied by the project. The native layer has no default project module or resolver. Each attachment creates an isolated Lua state, loads the module table with `require(moduleName)`, calls its `resolverName` function with `effectName`, then invokes the returned attacher as `(nil, control, sampleRate)` to obtain an `sf.SoundSource.EffectProcessor`. The isolated control exposes `isCancelled()`, `beginTail()` and `finishTail()`; it cannot attach another processor. Module loading, resolver and processor-creation failures propagate synchronously, without falling back to unprocessed audio. Errors raised later by the audio callback propagate through the Manager update.

The isolated state uses the module search path captured when AudioManager initialises. Configure any additional search paths before loading `GlobalCore`. Main-state closures and objects are not transferred to the audio state. The project owns its module names, resolver functions and preset registry; see [default gameplay audio](<../../../Default Gameplay/Resources Audio Video and Effects.md#audio>) for the supplied presets, playback lifetime and cancellation rules.

## FontManager

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `load` | `function` | filePath: string | return: sf.Font | — |
| `getFont` | `function` | fontName: string | return: sf.Font | — |
| `getFontFilename` | `function` | fontName: string | return: string | — |
| `getFontList` | `function` | — | return: string[] | — |
| `getFontFilenameList` | `function` | — | return: string[] | — |
| `hasFont` | `function` | fontName: string | return: bool | — |
| `getMemory` | `function` | — | return: int | — |

## ShaderManager

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `load` | `function` | shaderPath: string; shaderType: sf.Shader.Type = nil | return: sf.Shader | — |
| `loadFull` | `function` | vertPath: string; fragPath: string | return: sf.Shader | — |
| `loadFullShaderWithGeo` | `function` | vertPath: string; geoPath: string; fragPath: string | return: sf.Shader | — |
| `getMemory` | `function` | — | return: int | — |

## TextureManager

`load` returns a ready texture synchronously. `request` returns a `TextureLoadRequest` immediately, with file reading, archive decompression and image decoding performed in the background. The running System frame loop uploads decoded images to the GPU before publishing Ready. Requesting a texture does not make it drawable yet; check the handle from a later update instead of polling in a loop that blocks the frame.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `load` | `function` | filePath: string; sRGB: bool = false; area: sf.IntRect = nil; smooth: bool = false | return: sf.Texture | — |
| `request` | `function` | filePath: string; sRGB: bool = false; area: sf.IntRect = nil; smooth: bool = false; priority: int = 0 | return: GlobalCore.TextureLoadRequest | Lua only; `metadata = false` |
| `getPath` | `function` | texture: sf.Texture | return: string or nil | Lua only; `metadata = false` |
| `getMemory` | `function` | — | return: int | — |

`TextureManager.getPath(texture)` finds the actual texture instance in the manager's current cache and returns its original resource path. It performs no file read or load and does not retain the texture. A texture created outside the manager, or no longer present after cache clearing, returns `nil`. The returned path does not encode `load` options such as a cropped `area`, sRGB or smoothing.

Both loading methods share the same cache and in-flight work. Exact path, sRGB, area and smoothing distinguish cache entries; priority does not. Repeated requests for the same entry share decoding and upload, but receive independent subscription handles. A cache hit is immediately Ready. A synchronous `load` can finish a matching pending request without waiting for another frame, which also permits use before the System frame loop starts. Failed results are not reused, so a new request can retry the load.

### TextureLoadRequest

The handle retains its ready texture until it is cancelled or released. Other holders of that texture remain valid. The manager uses weak texture references on ordinary platforms and retains strong references on iOS until the cache is cleared.

| API | Contract |
|---|---|
| `getStatus()` | Return one of the read-only constants `TextureLoadRequest.PENDING` (`"pending"`), `READY` (`"ready"`), `FAILED` (`"failed"`) or `CANCELLED` (`"cancelled"`). This never waits or raises the stored load failure. |
| `getTexture()` | Return the ready `sf.Texture`, or raise the stored loading error for Failed. Pending and Cancelled raise a state error; this method never waits. |
| `setPriority(priority)` | Change this pending subscription's integer priority. Larger values run first, with request order breaking ties. Completed, failed and cancelled handles are unchanged. |
| `cancel()` | Release this subscription and make the handle Cancelled, including after Ready or Failed. Repeated cancellation is harmless. Textures already obtained from the handle remain valid. |

These methods and constants are Lua-only and do not create Blueprint nodes. A texture request is not an `Engine.AsyncOperation` and is not passed to `asyncio.await`. Poll its state from normal updates; call `getTexture()` for Ready or Failed so required-resource failures propagate.

The highest priority among pending subscriptions controls shared work. Cancelling or releasing one handle leaves other subscribers active. When the last subscriber leaves, queued work is cancelled and an in-flight result is discarded. An already-running decode or upload is not interrupted. Runtime shutdown cancels outstanding requests and joins the decoding workers before closing the graphics context.

The service uses two decoding workers. A 64 MiB backlog of decoded pixels pauses further background decoding; already-running decodes can still finish. Each frame starts at most two uploads, with an 8 MiB byte budget and a 1 ms time budget. A first texture larger than the byte budget is still allowed to make progress, and an upload already started cannot be interrupted. These are scheduling limits, not hard frame-time or total-memory limits: image decoding can hold additional pixel copies and codec buffers. The backlog counts the complete decoded image even when `area` selects a smaller GPU texture. Uploads pause while the mobile application is suspended.
