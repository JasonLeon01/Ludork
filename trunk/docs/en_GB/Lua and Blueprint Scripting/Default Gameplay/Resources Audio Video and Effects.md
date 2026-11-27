# Resources, Audio, Video and Effects

This page covers Default Gameplay's logical resource paths, animation caches, map audio and DSP effects, 2D materials, procedural fog, panoramas, composite-world environments and FFmpeg video playback.

## Resource paths

Stored assets use exact-case `/Game/Assets/...` paths with `/` separators and file extensions. Generic resource managers require this full path, whereas category helpers such as `loadCharacter("actors/Hero.png")` and `playSE("cursor.ogg")` accept category-relative paths and prepend their own prefix.

The runtime reads both loose files and `.ldpak` archives through the same logical paths, including streaming and seeking for music and video. Standard Lua filesystem APIs and LuaSF file constructors do not resolve paths inside archives.

## Animation caches

Animation sources live in `Data/Animations`. `Engine.getAnimationCacheRoot()` resolves and creates `Cache/Animations` under `Engine.getUserDataPath()` for both loose and archived data, following the [application user-data location](<../../Editor User Guide/Project Settings and Asset Layout.md#game-configuration>) in every launch mode.

- **Build and invalidation**: Startup generates missing `.anim.json`/`.anim.ldc` caches and rebuilds them whenever source definitions or referenced frame assets change, preserving relative paths under `Data/Animations`. Caches are disposable and omitted from templates and player packages.
- **Binary format**: Sources decode through `Engine.decodeData` and caches write through `Engine.saveData`. `.anim.ldc` stores direct binary data without intermediate JSON; delete legacy compressed-JSON caches so startup regenerates them.
- **Modules**: `Source.Utils.AnimationCache` handles key extraction, frame-asset discovery and staleness checks for `SceneInit`; `Global.Data.LoadAnimations()` loads prepared caches.

## Audio

Music, sound effects and voices each have separate enable switches and volumes in the [game configuration](<../../Editor User Guide/Project Settings and Asset Layout.md#game-configuration>). Play gameplay audio through the [GlobalFunctions Manager helpers](<../Global and Core Modules/Core Modules/GlobalFunctions.md#manager>).

### Map BGM and BGS (`Global.Audio.MapAudio`)

`Global.Audio.MapAudio` manages map BGM/BGS playback and filters using `Global.Audio.MapAudioState` for source reuse and fades. Custom scenes construct an instance with `MapAudio.new()`, trigger tracks via `playMapAudio(mapData, transitionSeconds?)`, advance fades in `onTick(deltaTime)` and halt playback with `stopMapAudio()`.

User music enable and volume settings apply immediately during fades, and updating a track's filter volume preserves fade progress. Requesting the already-playing file reuses its source, reapplies the filter and cancels any active fade. See [Script operations and lifetime](<../Source Classes/SceneMap.md#script-operations-and-lifetime>) for scene-stack returns.

### Lua DSP audio effects

`Source.Utils.AudioEffects` defines the case-sensitive DSP presets used by the gameplay Manager and Blueprint audio nodes:

| Preset | Default parameters |
|---|---|
| `Echo` | `0.3` s delay, `0.5` decay |
| `Distortion` | `2.0` drive, `0.7` threshold |
| `Underwater` | `0.7` depth, `0.3` bubble |
| `BehindWall` | `900` Hz cutoff, `0.35` transmission |

- **Selecting a preset**: `GlobalFunctions.Audio.SetEffect(audioType, effect)` resolves `effect` via `AudioEffects.Get` and stores the attacher independently for `Sound`, `Voice` or `Music` (both parameters appear as drop-downs in metadata). Selecting an effect does not instantiate a processor or alter playing sources; each subsequent source invokes the attacher in an isolated audio Lua state with a fresh processor. Pass the literal string `"nil"` to clear future playback; any other unknown name raises an error immediately.
- **Registry ownership**: `Source.Utils.AudioEffects` is the sole preset registry — adding or renaming presets also requires updating `SetEffect`'s metadata drop-down. The project attacher passes `"Source.Utils.AudioEffects"`, `"Get"` and the preset name to [`AudioEffectControl.attachLuaProcessor`](<../Global and Core Modules/Core Modules/GlobalCore/Resource Managers.md#audiomanager>); replacing the registry module requires updating that call. Effect selection is separate from `SoundFilter` and `MusicFilter`.
- **Sample rate, hot reload and tail draining**: Non-empty presets capture the active playback device's sample rate on source creation (active sources keep their coefficients across device rerouting; start a new Manager playback to adopt the new rate or pick up [hot-reloaded](<../Lua Runtime and Modules.md#lua-hot-reload>) audio code). Presets do not chain or mix. `Echo` drains its buffered tail after natural end of stream, whereas an explicit Manager stop, dialogue skip or shutdown discards the tail. Direct `sf.SoundSource:setEffectProcessor` calls bypass Manager isolation, EOS handling and cancellation.
- **Dialogue voice lifetime**: `GlobalFunctions.Scene.ShowVoiceMessageByTag` and `ShowVoiceMessage` track the Voice handle started by that call and stop it when dialogue ends before `FinishedDialogue` resumes downstream nodes (so a subsequent `SetEffect` applies to the next Voice). Leaving or destroying a map scene also stops the active global Voice; ordinary message nodes leave Voice playback untouched.

## Shaders, fog and transitions

### 2D materials and shaders

Actors and tile layers render through Ludork's 2D material system. `SceneInit` generates material shaders and runtime metadata in the user data directory before gameplay, followed by a separate [platform shader cache](<../../Native C++ Development/Build and Module Layout.md#mrb-rendering>) preparation stage; see [Runtime generation](<../../Editor User Guide/Materials.md#runtime-generation>) for cache validation and ownership.

- **Material combinations**: Startup prepares all individual materials and scans saved maps for tile/layer combinations. If runtime scripts construct maps or swap layer materials into new combinations, register them in [Game → Material Combinations](<../../Editor User Guide/Materials.md#material-combinations>) before preparation. Actor and UI material switches and parameter edits need no extra entry.
- **Sharing and evaluation**: Material resources and compiled programs are shared while per-instance parameters stay independent. Character effects are standard `Data/Materials` graphs (for example, hue shifts use a HueShift node and parameter). Alpha, light blocking and vertex displacement evaluate from the same graph, so a dissolved silhouette updates its shadow mask automatically. Emission composites within each surface without illuminating neighbours or bypassing foreground occlusion.
- **Built-in pipelines and platform support**: Built-in weather and fog use cross-platform shader pipelines whose load and compile errors propagate. World entry prewarms the shared renderer for the current viewport and repeats after regional geometry changes. HarmonyOS compiles programs on first normal draw once render state and uniforms are bound. Shader availability depends on the target runtime GPU — a working editor preview does not guarantee target hardware support.
- **Particles**: Attach GPU [Common Particle](<../../Editor User Guide/Common Particles.md>) emitters to Actor Components and UI; Default Gameplay's code-driven particles work alongside them.

### Enemy defeat shatter effect

Default Gameplay's project-wide defeat shatter effect is toggled by `Enemy.DefeatShatterEffectEnabled` in [Shared Gameplay configuration](<Runtime Data Configuration and Saves.md#shared-gameplay-configuration>) rather than per Enemy Blueprint. Its shader snapshots the enemy and scatters world-space logical-pixel fragments radially outwards over ~`0.65` seconds. Because the enemy is already recorded and destroyed, the effect is visual only and never blocks movement, collision or light. When disabled or when `sf.Shader.isAvailable()` is `false`, defeated enemies disappear immediately.

### Procedural fog and panoramas

- **GPU fog**: Generated procedurally from world coordinates, absolute time and a deterministic seed using four noise layers (no fog texture). Density controls coverage, colour alpha scales patch opacity without adding a flat veil, and smaller scales create finer patches (including for black fog). Map data stores `enabled`, `colour`, `density`, `scale`, `velocity`, `turbulence` and `seed` (see [FogController](<../Global and Core Modules/Core Modules/GlobalCore/Map and Fog.md#fogcontroller>)). Fog composites after materials and lighting, before foreground weather and particles, responds to scene light, and resets per map session.
- **Map panorama**: Optional canonical `/Game/Assets/...` string on `panorama` (missing, `nil` or blank disables it). The texture Cover-scales to `GameMap.MapViewRect` preserving aspect ratio and crops overflow by the Camera origin as a fraction of `(mapPixels - viewPixels)` per axis (`0.5` when an axis cannot scroll). Transparent tiles and canvas margins show the panorama tinted by ambient light only, while point lights apply to opaque tiles. A missing panorama texture fails loading.

### Composite-world environment

- **Fog and panorama**: A world manifest may set global fog (covering the whole world, including holes) and a global panorama (scrolling across total world bounds). Child fog clips to its placement, samples the same world coordinates and time, composites after global fog in stable placement order (blending continuously across matching neighbours), and disappears on child eviction until reload. A child's `panorama` applies only when loaded as a standalone map.
- **Audio and ambient light**: World manifests cannot define BGM, BGS, audio filters or ambient light — these follow the child map under the player, with `0.5`-second fades for audio and subsequent ambient-light transitions. Holes have no map audio and use white ambient light while keeping world fog and panorama active.
- **World lighting**: Lights operate in world coordinates, so Actor `LightComponent` lights illuminate and cast shadows across child boundaries. Tile light masks clip to their placements, and holes contribute neither surfaces nor blockers. See [PreviewHost: Map and World Previews](<../../PreviewHost/Map and World Previews.md>) for editor preview controls.

## Video

Video playback requires an FFmpeg-enabled project or template. Store videos under `Assets/Videos` and reference them as `/Game/Assets/Videos/...` (loose files and `Assets.ldpak` entries share the same seekable playback contract). See [FFmpeg option](<../../Getting Started/Project Templates.md#ffmpeg-option>) for build and packaging details.

- **Android**: Supports MOV, H.264/AAC, `swscale` and `swresample`; excludes network streams, JNI, MediaCodec and hardware acceleration.
- **WebGL**: Uses the same FFmpeg software decoder, resource paths, mute, skip and subtitles without relying on browser HTML codecs. Audio obeys [browser startup policy](<../../Editor User Guide/Run Debug and Package.md#webgl>), focus loss pauses the video clock, and decoding runs on the main thread (so smooth resolution depends on the device).

Play videos with `GlobalFunctions.Video.PlayVideo(videoFileName, mute, skipable, subtitleFileName?)`, where the optional fourth argument is a project-relative `Data/Subtitles/...json` path (omitted or `""` plays without subtitles):

```lua
local Video = require("GlobalFunctions.Video")
Video.PlayVideo("/Game/Assets/Videos/intro.mp4", false, true, "Data/Subtitles/intro.json")
```

[Subtitle files](<../../Editor User Guide/Video Subtitles.md>) define `type: "subtitle"` and a `sections` array with start/end times (seconds) plus either a shared string array or a language-keyed dictionary. Dictionary entries select the active game language, falling back to `en_GB` only if the key is absent (an explicit empty array suppresses subtitles). Subtitles track the video clock and render centred at the bottom of the viewport in white with a black outline using the default font.
