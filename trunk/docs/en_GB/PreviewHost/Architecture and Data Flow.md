# Architecture and Data Flow

PreviewHost renders editor content with the project's native implementation: the editor owns authoring, input and overlays, while PreviewHost owns native preview sessions and pixel transport. See [Map and World Previews](<Map and World Previews.md>) and [UI, Actor, Particle and Subtitle Previews](<UI Actor Particle and Subtitle Previews.md>) for view-level operation.

## Shared renderer and module boundary

- **Shared map renderer (`Engine/Source/Core/include/Graphics/MapRenderer.hpp`):** Engine and `PreviewHostRuntime` compile the same Core Graphics renderer. `Scene` holds ordered layers, tilemap regions, sprites, lights, ambient light, panorama and fog; `View` and `Options` supply camera, output size, time and display flags. It outputs an `sf::RenderTexture` without parsing editor requests or reading pixels to the CPU.
- **Map and world layers:** Ordinary maps use one region; worlds draw regions within each authoritative layer, preserving placement priority and cross-region lighting and occlusion. `GlobalCore.GameMapRenderer` adapts gameplay state and supplies emitter, billboard, layer-effect and ordinary-map player-cover hooks.
- **Tile updates and animation:** `TileLayer::updateBlock` rebuilds affected chunks and autotile neighbours while invalidating material and occlusion caches; streaming uses `writeBlock` and its normal build stages, and animation uses the explicit frame time.
- **Host isolation (`Engine/PreviewHost`):** `Engine/PreviewHost` owns request parsing, editor sessions, IPC, BGRA readback and temporary frame transport outside Core's recursive source collection. Game modules never include it or link `PreviewHostRuntime`, and `LudorkRuntime` keeps its downward dependency boundary without depending on map gameplay objects. See [native module layout](<../Native C++ Development/Build and Module Layout.md>) and [Build, Publish and Troubleshooting](<Build Publish and Troubleshooting.md>).

## Scene updates

- **Live in-memory snapshots:** The editor sends live in-memory map data—including unsaved edits, Blueprint instance overrides, referenced particle resources with resolved curves and material-document snapshots—within the [preview/gameplay boundary](<Overview.md#preview-and-gameplay>). Lights and Emitter effects preview even when their Actor has no texture. The native registry supplies the material node and parameter schema; the editor maintains no separate shader model.
- **Map material revisions:** Map requests carry a `materialRevision`. A complete `materials` snapshot accompanies the first request, material edits, undo, renames, deletions and resource refreshes; other frames and map switches reuse that snapshot and its parsed resources (a missing snapshot for the requested revision is an error). Material overrides apply only within their own preview request and never affect other preview types.
- **Material-window requests (`renderMaterial`):** Each request carries a `materialRevision` for the active material. The first request, content or path changes, and new connections send that material's `asset` and `materialPath` (excluding graph layout and unrelated materials); subsequent frames send only time and preview settings.
- **Material-window caching:** The session caches the parsed material, generated GLSL and compilation errors, updating the scene when preview settings or source textures change. Resending the same revision and path returns cached sources and diagnostics without recompiling, while detecting texture file changes between frames.
- **Material diagnostics (`diagnosticsRevision`):** Texture and frame-rectangle errors can appear or clear independently of material edits. Ordinary frames include diagnostics only on change (an empty array clears earlier errors), whereas snapshot responses always include diagnostics and generated sources.
- **Diagnostics retention and error recovery:** The client preserves diagnostics even when a newer edit invalidates the accompanying image. A failed material shows its error image instead of a previous valid render; fixing the graph or missing texture restores the preview.
- **Incremental map updates:** A full map scene initialises a session; later edits update scene fields, regions and identified sprite/light records, while camera and animation frames reuse the installed scene. Actor-only edits refresh cached visuals and lighting without rescanning terrain or rebuilding Tilemaps; structural changes and resource refreshes rebuild required state across ordinary maps, worlds and thumbnails.

### Renderer caching and statistics

- **Static light caching:** The Core renderer reuses render targets and light caches between frames. Static light results key on position, colour, radius, intensity and texture dimensions regardless of distance sorting; static occlusion changes invalidate affected cache entries while preserving reusable texture storage.
- **Dynamic occupancy and transmission:** Dynamic transmission and occupancy textures cache separately by exact dimensions, preserving lighting resolution and sampling coordinates. Static and dynamic occupancy share one reusable CPU pixel buffer; unchanged dimensions receive pixel updates, and dynamic occlusion redraws and uploads current pixels each frame.
- **Dynamic pool budget:** Each dynamic texture pool holds at most the shader light limit (64 in the shipped configuration), and pools share a 64 MiB RGBA pixel budget per renderer with least-recently-used eviction. Active-light textures stay alive until drawing finishes; over-budget requests allocate temporary storage reclaimed afterwards.
- **Static caches and CPU buffer:** The 64 MiB dynamic budget excludes driver and framebuffer overhead, static light caches and the CPU buffer. Static light caches cap at the shader light limit, and the CPU buffer preserves its peak capacity; working sets fitting within both limits are reused after warm-up, whereas larger sets may reallocate.

`MapRenderer::statistics()` and preview responses report these frame metrics:

| Metric | Meaning |
|---|---|
| `renderTextureAllocations` | Render targets created during the latest frame. |
| `occupancyTextureAllocations` | Static or dynamic occupancy textures created during the latest frame. |
| `occupancyBufferGrowths` | CPU occupancy buffer capacity growths during the latest frame. |
| `staticLightRedraws` | Static light redraws performed during the latest frame. |
| `residentRenderTextures` | Resident render targets held at frame end, including cached lighting targets. |
| `dynamicLightingCacheBytes` | Estimated RGBA pixel bytes held by dynamic lighting pools at frame end. |

Map switches and scene closure keep reusable textures, CPU buffer capacity, shaders and material programs while invalidating scene-dependent occlusion and lighting (`MapRenderer::invalidateScene()`; call `reset()` to release all renderer resources). Resource refreshes and connection shutdown release preview resources. Surface masks render only when a visible material needs reflections.

The renderer consumes plain scene, region, sprite, light and view records with no Actor or Lua dependencies. View mode, fog and panorama options operate independently (see [Map and World Previews](<Map and World Previews.md>)); runtime fog configuration and static floor-preview APIs are documented in [GlobalCore: Map and Fog](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/GlobalCore/Map and Fog.md>).

## Scheduling and frame ownership

- **Host processes:** The project map preview service starts separate PreviewHost processes as needed for map/world viewports and for serialised thumbnail rendering; other preview surfaces may open their own connections. On macOS, host processes run without Dock icons and leave keyboard focus with the active application. Thumbnail count never spawns additional processes.
- **Foreground coalescing:** At most one foreground request runs at a time, keeping only the latest pending state with animation capped at 30 FPS. A newly selected map's first frame bypasses the animation interval once the connection is free, though an in-flight request completes before the pending map is submitted.
- **Hidden views and thumbnails:** Hidden views stop refreshing, and active interactions pause new thumbnail tasks. Thumbnails render at time `0` and cache by scene/resources, size and display options.
- **Progressive map publication:** Compatible completed map frames may publish during continuous edits so newer pending edits do not starve the display, while scene-identity, resource and display changes invalidate incompatible results. Each published bitmap carries its frame's camera transform, and the final edit renders once interaction ends.
- **Material-window scheduling:** The material window schedules a `640 × 300` preview every 100 ms with at most one request in flight; hidden, minimised or disabled windows do not submit frames. Its `generation` identifies the active material and preview settings so obsolete images are discarded independently of the cached `materialRevision`, and layout-only graph changes do not replace the material snapshot.
- **Shared-memory pixel transport:** Each connection owns two reusable file mappings for BGRA frames, growing them only when capacity is insufficient. The host writes into mapped storage and holds pixels until the client copies all pages (including Actor batches); the editor reuses mapped views. Closing or rebuilding a connection releases its mappings and temporary files.
- **Thread ownership:** The UI captures an independent map request on the UI thread, runs serialisation, IPC, pixel reads and bitmap creation off-thread (background work never reads the editable scene) and publishes an immutable bitmap with its camera transform. These responsibilities belong exclusively to PreviewHost and the editor, never the game runtime.

## Capabilities and timing

The editor validates the host protocol and advertised capabilities before submitting requests:

| Surface | Required capabilities |
|---|---|
| All previews (transport) | `persistentFrames` |
| Map editing | `map`, `mapActorChanges`, `mapEmitters`, `mapMaterialSnapshots` |
| Material window | `material`, `materialSnapshots` |

An incompatible host must be rebuilt or updated; map previews have no Avalonia fallback renderer. See [Build, Publish and Troubleshooting](<Build Publish and Troubleshooting.md#troubleshooting>).

`MapPreviewLease.LastFrameTiming` exposes change-to-publication latency alongside per-stage timings: scene update, serialisation, native scene application, rendering, readback, transfer, client pixel read and bitmap creation. Its nullable `SelectionToPublishMilliseconds` measures from map selection (before document preparation) to the first compatible published frame; discarded frames do not consume it, and later animation frames leave it unset.

Both publication timings end when the editor publishes the frame rather than when the display presents it. Compare stages at identical scene, camera and output dimensions before attributing latency to native drawing: a `3840 × 2160` frame carries over 33 MB of BGRA pixel data before transport overhead, regardless of map tile dimensions.
