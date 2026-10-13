# Resources, Audio, Video and Effects

## Resource paths

Stored resources use exact-case `/Game/Assets/...` paths with `/` separators and extensions. Generic managers require that complete form. Category helpers such as `loadCharacter("actors/Hero.png")` and `playSE("cursor.ogg")` accept only category-relative arguments and add their own prefix.

The runtime reads loose files and `.ldpak` entries through the same logical paths, with streaming and seeking for music and video. Standard filesystem APIs and LuaSF file constructors do not resolve paths inside archives.

## Animation caches

Animation sources live in `Data/Animations`. `Engine.getAnimationCacheRoot()` creates `Cache/Animations` under `Engine.getUserDataPath()` for both loose and archived Data. On Windows, the user-data root is the runtime working directory. Other platforms use their application user-data location.

Initialisation generates missing `.anim.json`/`.anim.ldc` caches and rebuilds them when source data or referenced frame assets change, preserving paths relative to `Data/Animations`. Caches are disposable and excluded from templates and player packages. Sources are decoded through `Engine.decodeData` and caches are written through `Engine.saveData`; `.anim.ldc` contains binary data, with no intermediate JSON conversion. Delete old caches containing compressed JSON so initialisation regenerates them.

## Audio

Music, sound effects and voices have independent enable flags and volumes in `Main.ini`. The GlobalFunctions Manager group provides the gameplay-facing helpers. Handle missing optional sound paths deliberately. Repeatedly loading the same resource under different path spellings defeats caching.

Audio effects used by the gameplay Manager and Blueprint nodes are Lua DSP processors defined by `Source.Utils.AudioEffects`. The Game project provides the case-sensitive `Echo`, `Distortion`, `Underwater` and `BehindWall` presets. Their defaults remain a 0.3-second/0.5-decay echo, a 2.0-drive/0.7-threshold distortion, a 0.7-depth/0.3-bubble underwater effect, and a 900-Hz/0.35-transmission behind-wall filter.

`GlobalFunctions.Audio.SetEffect(audioType, effect)` resolves the name with `AudioEffects.Get` and stores the resulting attacher independently for the `Sound`, `Voice` or `Music` category. Metadata presents both parameters as drop-downs. Selection does not create a processor and does not modify a source that is already playing. Each later source invokes the attacher, which creates an isolated audio Lua state and a fresh processor for that source. Select the literal string `nil` to clear the preset for future playback. Any other unknown name raises immediately. `Source.Utils.AudioEffects` is the sole preset registry, so a change to the preset registry also requires an update to the `SetEffect` metadata drop-down. Effect selection is not part of `SoundFilter` or `MusicFilter`.

For a non-empty preset, source creation captures the current playback-device sample rate before play. An active source keeps those coefficients after device rerouting, so start a new Manager playback to use the new rate. The attacher must install exactly one non-empty processor, so a selected preset cannot play dry. SFML stores one processor per source. Presets are not chained or mixed.

After hot reload, start a new playback to use updated audio code; see [Lua Runtime and Modules](<../Lua Runtime and Modules.md#lua-hot-reload>). Playing sources and buffered tails continue unchanged.

`Echo` drains its buffered tail after a natural end of stream. An explicit Manager stop, a dialogue skip or shutdown discards the remaining tail. Direct `sf.SoundSource:setEffectProcessor` use is a separate low-level API, and it does not receive the Manager isolation, EOS or cancellation protocol.

`GlobalFunctions.Scene.ShowVoiceMessageByTag` and `ShowVoiceMessage` capture the exact Voice handle started by that call for the lifetime of their dialogue. When the dialogue finishes, their latent condition stops that handle before `FinishedDialogue` resumes downstream Blueprint execution. A following `SetEffect` therefore configures the next Voice. Leaving or destroying a map scene also stops the current global Voice. Ordinary message nodes do not change Voice playback.

## Shaders, fog and transitions

User-authored Actor and layer shaders remain optional. Check `sf.Shader.isAvailable()` or `sf.Shader.isGeometryAvailable()` when gameplay can run without them. Built-in weather and fog instead require their shared cross-platform shader assets and propagate load or compilation failures. World entry prewarms the shared map compositor for the current viewport and repeats when regional geometry changes. HarmonyOS skips explicit prewarming and compiles each shader during its first normal draw, after the complete render state and uniforms are bound. HarmonyOS OpenGL ES treats unversioned shaders as ESSL 1.00 and preserves the public `setUniform("texture", ...)` name. Explicit versions must match the backend.

Actor fragment shaders are cached by exact shader path for the lifetime of the Engine runtime. Actors using the same path therefore receive the same `sf.Shader` from `Actor:getShader()`. The standard `texture`, `time`, `textureSize` and `textureRect` uniforms are rebound immediately before each Actor map or preview draw. Other uniforms remain shared mutable shader state and must not be treated as persistent per-Actor values. A shader that fails to load is not cached. The affected Actor reports `Actor:hasShaderError()` and continues through the existing fallback rendering path.

Fog is generated on the GPU from world coordinates, absolute time and a deterministic seed. Four noise layers produce sparse and dense patches; density controls coverage and colour alpha scales their opacity without adding a uniform veil. Smaller scales produce smaller patches, including for black fog. Map data store an object with `enabled`, `colour`, `density`, `scale`, `velocity`, `turbulence` and `seed`; there is no fog texture. The shared renderer composites fog after materials and lighting, before foreground weather and particles. Lit fog responds to scene light; Unlit fog retains its configured colour. Each map session owns its fog state.

See [FogController](<../Global and Core Modules/Core Modules/GlobalCore/Map and Fog.md#fogcontroller>) for fog fields, defaults and accepted values.

Map panorama data uses an optional-string protocol. `panorama` is a canonical `/Game/Assets/...` path, and panorama is disabled when it is missing, `nil` or blank. The image Cover-scales to fill `GameMap.MapViewRect` while preserving the aspect ratio, and then crops the overflow using the Camera origin as a fraction of `(mapPixels - viewPixels)` on each axis. An axis with no camera travel uses `0.5`. Transparent tiles and canvas margins reveal the panorama multiplied by ambient light only, while point lights still apply to opaque tiles. A missing texture is a required-resource failure.

### Composite-world environment

A world manifest may define global fog and a global panorama, but not BGM, BGS, audio filters or ambient light. Global fog covers the world bounds, including holes. Child fog is clipped to its placement and sampled in the same world coordinates and time. It is composed after global fog in stable placement order; matching settings remain continuous across adjacent regions. Evicting a child removes its regional fog until reload. Global panorama covers the world Camera using the world size for scroll. A child's `panorama` applies only when that child is loaded as a standalone map.

BGM, BGS and ambient light follow the child beneath the player. Audio switches fade for 0.5 seconds, and returning during a fade-out restores that source. Later ambient changes also blend for 0.5 seconds, using white in holes. World fog and panorama remain active there.

The Game project's advanced enemy-defeat effect is controlled by the global class setting `Enemy.DefeatShatterEffectEnabled`, which defaults to `true` and is not a Blueprint field. Its shader renders a snapshot as world-space logical-pixel fragments scattering radially outward from the centre for about 0.65 seconds. The defeated enemy has already been recorded and destroyed, so the snapshot does not affect passability, collision or light blocking. When the setting is `false` or `sf.Shader.isAvailable()` is false, the enemy disappears immediately instead.

## Video

Video playback requires an FFmpeg-enabled project or template. Keep files beneath `Assets/Videos` and reference them as `/Game/Assets/Videos/...`. Loose files and entries in the root `Assets.ldpak` share the same seekable playback contract. Android supports MOV, H.264/AAC, swscale and swresample, but not network, JNI, MediaCodec or hardware acceleration. Build and distribution requirements are in [Project Templates](<../../Getting Started/Project Templates.md#ffmpeg-option>).

Call `Video.PlayVideo(videoFileName, mute, skipable, subtitleFileName)` through `require("GlobalFunctions.Video")`. The optional fourth argument is a project-relative `Data/Subtitles/...json` path; omission or an empty string preserves video-only playback.

```lua
local Video = require("GlobalFunctions.Video")
Video.PlayVideo("/Game/Assets/Videos/intro.mp4", false, true, "Data/Subtitles/intro.json")
```

[Subtitle data](<../../Editor User Guide/Video Subtitles.md>) contain `type: "subtitle"` and a `sections` array. Each section supplies start/end times in seconds and either a shared string array or a language-to-array dictionary. Dictionary content selects the current game language, then `en_GB` only if the requested key is missing; an existing empty array suppresses subtitles. Subtitles follow the video clock and display centred at the bottom of the viewport using the default font, white text and a black outline.

## Limitations

Shader availability is determined by the runtime graphics environment, and a successful editor preview does not guarantee equivalent target hardware support.

World lighting uses world coordinates, so Actor lights illuminate and cast shadows across child boundaries. Tile masks remain clipped to their placements, and holes contribute neither surfaces nor blockers. Fixed map light records and their map management APIs are removed; attach a `LightComponent` to an Actor to create a light.

Editor ordinary maps, world canvases and thumbnails use the same native map renderer. Unlit is the default: white ambient and no direct light or shadows, with opacity, reflections, shaders and hue preserved. Lit uses actual light inputs, and the editor world canvas uses each child's ambient colour. Panorama and fog have independent display switches. The game retains its player-region ambient transition described above.

## Related pages

- [Common Particles](<../../Editor User Guide/Common Particles.md>) — independent GPU emitter assets for Actor Components and UI; existing code-driven particles and effects remain available.
- [Project Templates](<../../Getting Started/Project Templates.md>)
- [Run, Debug and Package](<../../Editor User Guide/Run Debug and Package.md>)
- [GlobalFunctions Lua API](<../Global and Core Modules/Core Modules/GlobalFunctions.md>)
