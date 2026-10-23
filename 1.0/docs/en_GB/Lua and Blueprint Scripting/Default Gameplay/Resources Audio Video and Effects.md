# Resources, Audio, Video and Effects

## Resource paths

Stored resources use exact-case `/Game/Assets/...` paths with `/` separators and extensions. Generic managers require that complete form. Category helpers such as `loadCharacter("actors/Hero.png")` and `playSE("cursor.ogg")` accept only category-relative arguments and add their own prefix.

The runtime reads loose files and `.ldpak` entries through the same logical paths, with streaming and seeking for music and video. Standard filesystem APIs and LuaSF file constructors do not resolve paths inside archives.

## Animation caches

Animation sources live in `Data/Animations`. `Engine.getAnimationCacheRoot()` creates `Cache/Animations` under `Engine.getUserDataPath()` for both loose and archived Data. All launch modes use the [application user-data location](<../../Editor User Guide/Project Settings and Asset Layout.md#game-configuration>).

Initialisation generates missing `.anim.json`/`.anim.ldc` caches and rebuilds them when source data or referenced frame assets change, preserving paths relative to `Data/Animations`. Caches are disposable and excluded from templates and player packages. Sources are decoded through `Engine.decodeData` and caches are written through `Engine.saveData`; `.anim.ldc` contains binary data, with no intermediate JSON conversion. Delete old caches containing compressed JSON so initialisation regenerates them.

`Source.Utils.AnimationCache` provides source-key extraction, frame-asset discovery and staleness checks for the loading scene. `Global.Data.LoadAnimations()` loads the prepared caches.

## Audio

Music, sound effects and voices have independent enable flags and volumes in the [game configuration](<../../Editor User Guide/Project Settings and Asset Layout.md#game-configuration>). Use the [GlobalFunctions Manager helpers](<../Global and Core Modules/Core Modules/GlobalFunctions.md#manager>) for gameplay playback.

`Global.Audio.MapAudio` manages map BGM/BGS playback and filters, using `Global.Audio.MapAudioState` for source reuse and fades. A custom scene creates an instance with `MapAudio.new()`, calls its `playMapAudio(mapData, transitionSeconds?)`, advances transitions through `onTick(deltaTime)` and stops playback with `stopMapAudio()`.

User music enable and volume settings take effect immediately during fades. Changing a track's filter volume preserves fade progress. Requesting the current file again reuses its source, reapplies the requested filter and cancels the fade. For scene-stack returns, see [SceneMap lifetime](<../Source Classes/SceneMap.md#script-operations-and-lifetime>).

Audio effects used by the gameplay Manager and Blueprint nodes are Lua DSP processors defined by `Source.Utils.AudioEffects`. The Game project provides the case-sensitive `Echo`, `Distortion`, `Underwater` and `BehindWall` presets. Their defaults remain a 0.3-second/0.5-decay echo, a 2.0-drive/0.7-threshold distortion, a 0.7-depth/0.3-bubble underwater effect, and a 900-Hz/0.35-transmission behind-wall filter.

`GlobalFunctions.Audio.SetEffect(audioType, effect)` resolves the name with `AudioEffects.Get` and stores the resulting attacher independently for the `Sound`, `Voice` or `Music` category. Metadata presents both parameters as drop-downs. Selection does not create a processor and does not modify a source that is already playing. Each later source invokes the attacher, which creates an isolated audio Lua state and a fresh processor for that source. Select the literal string `nil` to clear the preset for future playback. Any other unknown name raises immediately. `Source.Utils.AudioEffects` is the sole preset registry, so a change to the preset registry also requires an update to the `SetEffect` metadata drop-down. Effect selection is not part of `SoundFilter` or `MusicFilter`.

For a non-empty preset, source creation captures the current playback-device sample rate before play. An active source keeps those coefficients after device rerouting, so start a new Manager playback to use the new rate. The attacher must install exactly one non-empty processor, so a selected preset cannot play dry. SFML stores one processor per source. Presets are not chained or mixed.

After hot reload, start a new playback to use updated audio code; see [Lua Runtime and Modules](<../Lua Runtime and Modules.md#lua-hot-reload>). Playing sources and buffered tails continue unchanged.

`Echo` drains its buffered tail after a natural end of stream. An explicit Manager stop, a dialogue skip or shutdown discards the remaining tail. Direct `sf.SoundSource:setEffectProcessor` use is a separate low-level API, and it does not receive the Manager isolation, EOS or cancellation protocol.

`GlobalFunctions.Scene.ShowVoiceMessageByTag` and `ShowVoiceMessage` capture the exact Voice handle started by that call for the lifetime of their dialogue. When the dialogue finishes, their latent condition stops that handle before `FinishedDialogue` resumes downstream Blueprint execution. A following `SetEffect` therefore configures the next Voice. Leaving or destroying a map scene also stops the current global Voice. Ordinary message nodes do not change Voice playback.

## Shaders, fog and transitions

Actors and tile layers draw through the official 2D material system; projects compose effects with general-purpose nodes. Export writes the material shaders and metadata to `Assets/Shaders/MaterialGenerated`. The game loads those files and evaluates the exported numeric analysis instructions with current instance parameters, without parsing graphs or assembling GLSL; the GPU driver still compiles the shaders. Missing or invalid generated files stop material loading, so export before running or packaging.

Export includes all individual materials and scans saved maps for tile/layer combinations. If gameplay creates a different combination by constructing a map or changing a layer's material, add it through [Game → Material Combinations](<../../Editor User Guide/Materials.md#material-combinations>) before Export. Actor/UI material switches and parameter changes need no extra entry.

Built-in weather and fog retain their cross-platform shader pipelines, and loading or compilation errors propagate. World entry prewarms the shared renderer for the current viewport and repeats after regional geometry changes. HarmonyOS compiles programs on their first normal draw after complete render-state and uniform binding.

Material resources and generated programs can be shared while instance parameters remain independent; each draw binds that object's effective values. Character effects are ordinary graphs under `Data/Materials`, and hue is expressed with a HueShift node and parameter. Alpha, light blocking and vertex displacement use the relevant branches of the same graph, so a dissolved silhouette does not retain the original mask. Emission is composed within each surface; it neither illuminates nearby objects nor bypasses foreground occlusion.

Fog is generated on the GPU from world coordinates, absolute time and a deterministic seed. Four noise layers produce sparse and dense patches; density controls coverage and colour alpha scales their opacity without adding a uniform veil. Smaller scales produce smaller patches, including for black fog. Map data store an object with `enabled`, `colour`, `density`, `scale`, `velocity`, `turbulence` and `seed`; there is no fog texture. The shared renderer composites fog after materials and lighting, before foreground weather and particles. Fog responds to scene light. Each map session owns its fog state.

See [FogController](<../Global and Core Modules/Core Modules/GlobalCore/Map and Fog.md#fogcontroller>) for fog fields, defaults and accepted values.

Map panorama data uses an optional-string protocol. `panorama` is a canonical `/Game/Assets/...` path, and panorama is disabled when it is missing, `nil` or blank. The image Cover-scales to fill `GameMap.MapViewRect` while preserving the aspect ratio, and then crops the overflow using the Camera origin as a fraction of `(mapPixels - viewPixels)` on each axis. An axis with no camera travel uses `0.5`. Transparent tiles and canvas margins reveal the panorama multiplied by ambient light only, while point lights still apply to opaque tiles. A missing texture is a required-resource failure.

### Composite-world environment

A world manifest may define global fog and a global panorama, but not BGM, BGS, audio filters or ambient light. Global fog covers the world bounds, including holes. Child fog is clipped to its placement and sampled in the same world coordinates and time. It is composed after global fog in stable placement order; matching settings remain continuous across adjacent regions. Evicting a child removes its regional fog until reload. Global panorama covers the world Camera using the world size for scroll. A child's `panorama` applies only when that child is loaded as a standalone map.

BGM, BGS and ambient light follow the child beneath the player. Audio and later ambient-light changes fade for 0.5 seconds. Holes have no map audio and use white ambient light; world fog and panorama remain active there.

The Game project's advanced enemy-defeat effect is controlled project-wide by [Gameplay configuration](<Runtime Data Configuration and Saves.md#shared-gameplay-configuration>), not by an individual Enemy Blueprint. Its shader renders a snapshot as world-space logical-pixel fragments scattering radially outward from the centre for about 0.65 seconds. The defeated enemy has already been recorded and destroyed, so the snapshot does not affect passability, collision or light blocking. When `Enemy.DefeatShatterEffectEnabled` is `false` or `sf.Shader.isAvailable()` is false, the enemy disappears immediately instead.

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

World lighting uses world coordinates, so Actor lights illuminate and cast shadows across child boundaries. Tile masks remain clipped to their placements, and holes contribute neither surfaces nor blockers. Attach a `LightComponent` to an Actor to create a light.

Editor display options are described in [PreviewHost: Map and World Previews](<../../PreviewHost/Map and World Previews.md>).

## Related pages

- [Common Particles](<../../Editor User Guide/Common Particles.md>) — independent GPU emitter assets for Actor Components and UI; existing code-driven particles and effects remain available.
- [Run, Debug and Package](<../../Editor User Guide/Run Debug and Package.md>)
