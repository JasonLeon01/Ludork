# Architecture and Data Flow

PreviewHost renders editor content with the project's native implementation. The editor owns authoring, input and overlays; the host owns native preview sessions and pixel transport. See [Map and World Previews](<Map and World Previews.md>) and [UI, Actor, Particle and Subtitle Previews](<UI Actor Particle and Subtitle Previews.md>) for operation of each view.

## Shared renderer and module boundary

`Engine/Source/Core/include/Graphics/MapRenderer.hpp` defines the common native map renderer. Engine and `PreviewHostRuntime` compile the same Core Graphics implementation. `Scene` contains ordered layers, placed tilemap regions, resolved sprite appearances, component lights, ambient light, panorama and fog. `View` and `Options` supply the camera, output dimensions, time and display options. The result is an `sf::RenderTexture`; the renderer does not parse editor requests or read pixels back to the CPU.

Ordinary maps use one region. Worlds draw regions within each authoritative layer, preserving placement priority and cross-region lighting and occlusion. `GlobalCore.GameMapRenderer` adapts gameplay state and supplies emitter, billboard, layer-effect and ordinary-map player-cover hooks. Native `TileLayer::updateBlock` rebuilds affected chunks and autotile neighbours and invalidates material and occlusion caches. Streaming retains `writeBlock` and its normal build stages. Animation uses the explicit frame time.

`Engine/PreviewHost` owns request parsing, editor sessions, process communication, BGRA readback and temporary frame transport, outside Core's recursive source collection. Game modules neither include this implementation nor link `PreviewHostRuntime`. `LudorkRuntime` keeps its downward dependency boundary without depending on map gameplay objects. The [native module layout](<../Native C++ Development/Build and Module Layout.md>) describes the game modules; [Build, Publish and Troubleshooting](<Build Publish and Troubleshooting.md>) covers preview targets and distribution.

## Scene updates

The editor sends current in-memory map data, including unsaved edits and Blueprint instance overrides. Material, Actor, map and UI requests include the current material-document snapshots, so graph edits and exposed defaults appear without saving. The native registry supplies the material node and parameter schema; the editor does not maintain a separate shader model. It resolves visual records without running Actor creation, Tick or Blueprint gameplay events. Lights remain visual records even when their Actor has no texture.

A full scene initialises a session. Subsequent changes update scene fields, regions and identified sprite/light records; camera and animation requests reuse the installed scene. Actor-only edits update cached visual records and affected lighting without rescanning terrain or reconstructing Tilemaps. Structural changes and resource refreshes rebuild the necessary scene state. The same rendering path handles ordinary maps, world canvases and thumbnails.

The Core renderer retains render targets and light caches between frames. It accepts scene, region, sprite, light and view records without Actor or Lua dependencies. View mode, fog and panorama options affect the rendered image independently; their operation is documented in [Map and World Previews](<Map and World Previews.md>). Runtime fog configuration and static floor-preview APIs remain in [GlobalCore: Map and Fog](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/GlobalCore/Map and Fog.md>).

## Scheduling and frame ownership

The project map preview service lazily opens a foreground connection for map/world viewports and a background connection that serialises thumbnail work. Thumbnail count does not create additional processes. A single foreground request runs at a time, with only the latest pending state retained and animation capped at 30 FPS. Hidden views stop refreshing; interactions pause new thumbnail tasks. Thumbnails use time zero and cache by scene/resources, size and display options.

Compatible completed frames may be published during continuous edits so that newer pending changes do not starve the display. Scene identity, resource and display changes invalidate incompatible results. Each published bitmap carries the camera transform for that frame, and the last edit is rendered after interaction ends.

Preview connections own two reusable file mappings for BGRA frames, growing them only when capacity is insufficient. A response retains its pixels until the client has copied all of its pages, including Actor batches. The host converts into mapped storage; the editor reuses mapped views. Map pixel reads and bitmap creation run off the UI thread; the UI publishes an independent immutable bitmap with its camera transform. Closing or rebuilding a connection releases its mappings and temporary files. These responsibilities remain in the preview host and editor, not the game runtime.

## Capabilities and timing

The connection validates the host protocol and advertised capabilities. `persistentFrames` is required for preview transport; map editing also requires `map` and `mapActorChanges`. An older host must be rebuilt or updated; map previews do not fall back to Avalonia drawing. See [Build, Publish and Troubleshooting](<Build Publish and Troubleshooting.md#troubleshooting>).

`MapPreviewLease.LastFrameTiming` exposes change-to-publication latency and the scene update, serialization, native scene application, rendering, readback, transfer, client pixel read and bitmap creation stages. Change-to-publication ends when the editor publishes the frame; it does not measure physical input-to-display latency. Compare stages at the same scene, camera and output dimensions before attributing a delay to native drawing. A 3840 × 2160 frame has more than 33 MB of BGRA pixel data before transport overhead, regardless of the map's tile dimensions.
