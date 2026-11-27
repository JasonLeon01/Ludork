# Map and World Previews

The map viewport, world composition canvas and child-map thumbnails share Ludork's native game renderer so lighting, materials, fog, panorama and particles match the runtime. Use **Viewport Options** and editor overlays to inspect maps while authoring.

## Native map view

Ordinary maps, the world composition canvas and child-map thumbnails share the game renderer, including animated tiles and Actors, Emitter components, tile, layer and Actor materials, lighting, fog and panorama. Previewing resolves current in-memory map, Blueprint, material, particle and curve data—including unsaved changes and instance overrides—within the [preview/gameplay boundary](<Overview.md#preview-and-gameplay>).

Open a map or world from the **Map List** to preview it. Painting, selection, Actor transforms, Undo and world placement are described in [Tilesets, Autotiles and Maps](<../Editor User Guide/Tilesets Autotiles and Maps.md>); for runtime preparation and unavailable-host messages, see [Build, Publish and Troubleshooting](<Build Publish and Troubleshooting.md>).

An Actor's configured [Emitter component](<../Editor User Guide/Actor Components/Emitter.md#configuration>) follows its transform and local bounds (including Actors without a texture) with matching anchor, offset, rotation, scale and before/after draw order.

Moving an Actor, panning, zooming and ordinary scene refreshes preserve particle playback. Editing a particle resource or a referenced curve restarts only the affected effects, and Undo/Redo updates the preview; missing or invalid resources produce a preview error.

## Viewport options

The always-visible drop-down at the upper left of the ordinary-map or world canvas opens **Viewport Options**:

| Option | Default | Behaviour |
|---|---|---|
| View Mode | Unlit | **Unlit** uses neutral lighting; **Lit** displays ambient light, Actor lights and occlusion. |
| Realtime | On | Advances time for animation, particles and fog. Turning it off freezes preview time while edits refresh the image. |
| Show Fog | On | Displays the map's configured fog in either view mode. |
| Show Panorama | On | Displays the configured panorama in either view mode. |
| Show Light Bounds | Off | Displays light-range helpers in **Unlit**; disabled in **Lit** without clearing the saved choice. |
| Show Actor Origins | On | Displays Actor origin helpers. |

**Unlit** disables ambient-light tinting, point lights and shadows while preserving texture alpha, material colour effects, local emission and reflections. **Lit** displays actual illumination without extra light-range circles.

Fog uses scene lighting in **Lit** mode and its own colour under neutral lighting in **Unlit** mode. Fog and panorama switches do not create effects the map has not configured; see [fog settings](<../Editor User Guide/Tilesets Autotiles and Maps.md#fog>) and [panorama authoring](<../Editor User Guide/Tilesets Autotiles and Maps.md#map-list-and-composite-worlds>).

All six options are shared across the project and saved immediately in `Main.proj` under `editor.mapViewport`. Switching maps preserves them, and reopening the project restores them; they do not modify map data or create Undo steps. Projects without saved options use the defaults above.

Emitter playback starts at time `0` when opening another map or world. It pauses while **Realtime** is off or the viewport is hidden, and resumes without catching up elapsed pause time; moving an Actor while paused does not trigger distance-based emission on resume.

## Actor and layer assistance

In Actor mode on an ordinary map or an independently opened world child map, every selected Actor (single or multiple selection) displays a yellow outline over a black backdrop above the map render. The outline follows translation, rotation and scale; Actors with missing textures or degenerate dimensions show a grid-cell marker.

Selection markers stay visible when **Show Actor Origins** is off. See [Selecting and editing Actors](<../Editor User Guide/Tilesets Autotiles and Maps.md#selecting-and-editing-actors>) for selection and batch-editing gestures.

**Show Light Bounds** draws read-only ranges with a black outline, white dashes, a faint light-coloured fill and a contrasting centre marker without affecting Actor selection or editing gestures. Lights on Actors without a texture are also previewed.

Actor scaling moves the emitter position, whereas the circular range uses `lightRadius` in map pixels and scales only with editor zoom. Configure emitters and occlusion through [Light Component](<../Editor User Guide/Actor Components/Light.md>) and [Lights and Actors](<../Editor User Guide/Tilesets Autotiles and Maps.md#lights-and-actors>).

When an Actor's resolved `visible` property (including inherited defaults and instance overrides) is `false`, ordinary-map and world previews draw it at 25% opacity as an editor aid:

- **Opacity and overlays:** Texture alpha, material opacity and the 50% dimming of non-current layers on an ordinary map multiply this opacity. Selection outlines and error markers stay fully opaque, and the Actor stays selectable and movable.
- **Lighting and layers:** Invisible Actors contribute no lighting or light occlusion. Hidden layers are not drawn, while placement ghosts and missing-texture placeholders use their normal appearance. At runtime, invisible Actors are hidden.
- **Emitters:** Emitter effects receive the owning Actor's 25% visibility aid and non-current-layer dimming. Unlike runtime playback of an invisible Actor, preview emitters play while **Realtime** is on; hidden layers do not display their effects.

Layer dimming preserves authored layer order without reducing real light or occlusion strength. Selection markers, Actor origins and light ranges are editor overlays and never appear in thumbnails.

## Worlds and thumbnails

The world canvas and child-map thumbnails populate as child maps load; a missing or unreadable child does not prevent others from appearing. Editing maps or changing images refreshes affected previews.

Only children required for the visible area and its lighting are loaded for the canvas, so Emitter effects on unloaded children are not previewed. World rendering draws across regions per layer and evaluates cross-map lighting and occlusion, giving the selected placement priority within each layer.

Child-map thumbnails render a single-region scene at time `0` following **View Mode**, **Show Fog** and **Show Panorama**. Emitter effects stay at their initial state rather than playing continuously or advancing. See [Scheduling and frame ownership](<Architecture and Data Flow.md#scheduling-and-frame-ownership>) for caching and background scheduling.

World fog and region fog share world coordinates and time: global fog composites first, followed by region fog in stable placement order so adjacent regions with identical settings form a continuous pattern. A world uses its global panorama, while a child map opened independently uses its own panorama. Cover scaling and camera scrolling apply to panorama previews.

## Refresh and Live Debug

The native viewport refreshes at up to 30 FPS during dragging without reducing output resolution, displaying the final state after release. Hidden views stop refreshing. See [Scheduling and frame ownership](<Architecture and Data Flow.md#scheduling-and-frame-ownership>) for request handling and frame publication.

During **Individual Window** Play with **Live Debug**, the native viewport mirrors live Actors' appearances, transforms, materials and lights together with the map's fog, panorama and ambient light (world children use world coordinates for these effects), excluding Emitter effects.

**Viewport Options** affect only the editor image without altering the running game or granting editing permissions, and hidden Actors follow the visibility-aid rules above. See [Live Debug](<../Editor User Guide/Run Debug and Package.md#live-debug>) for available operations and the save boundary.

Embedded game execution hides the preview menu because that viewport displays the running game directly.
