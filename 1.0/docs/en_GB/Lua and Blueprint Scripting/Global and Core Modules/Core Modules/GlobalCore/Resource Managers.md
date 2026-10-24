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

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `load` | `function` | filePath: string; sRGB: bool = false; area: sf.IntRect = nil; smooth: bool = false | return: sf.Texture | — |
| `getPath` | `function` | texture: sf.Texture | return: string or nil | Lua only; `metadata = false` |
| `getMemory` | `function` | — | return: int | — |

`TextureManager.getPath(texture)` finds the actual texture instance in the manager's current cache and returns its original resource path. It performs no file read or load and does not retain the texture. A texture created outside the manager, or no longer present after cache clearing, returns `nil`. The returned path does not encode `load` options such as a cropped `area`, sRGB or smoothing.
