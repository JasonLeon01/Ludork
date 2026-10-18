# Map and World Previews

## Native map view

Ordinary maps, the world composition canvas and child-map thumbnails share the game renderer, including animated tiles and Actors, tile, layer and Actor materials, lighting, fog and panorama. Previewing resolves the current in-memory map, Blueprint and material data, including unsaved changes and instance overrides, within the [preview/gameplay boundary](<Overview.md#preview-and-gameplay>).

Open a map or world from the Map List to see its preview. Painting, selection, Actor transforms, Undo and world placement are described in [Tilesets, Autotiles and Maps](<../Editor User Guide/Tilesets Autotiles and Maps.md>). For runtime preparation and unavailable-host messages, see [Build, Publish and Troubleshooting](<Build Publish and Troubleshooting.md>).

## Viewport options

The fixed, compact dropdown at the upper left of the ordinary-map or world canvas opens **Viewport Options**.

| Option | Default | Behaviour |
|---|---|---|
| View Mode | Unlit | Unlit uses neutral lighting; Lit shows ambient light, Actor lights and their occlusion. |
| Realtime | On | Advances animation and fog time. Turning it off freezes preview time while edits still refresh the image. |
| Show Fog | On | Shows the map's configured fog in either view mode. |
| Show Panorama | On | Shows the configured panorama in either view mode. |
| Show Light Bounds | Off | Shows light-range helpers in Unlit; disabled in Lit without clearing the saved choice. |
| Show Actor Origins | On | Shows Actor origin helpers. |

Unlit disables ambient-light tinting, point lights and shadows while retaining texture alpha, material colour effects, local emission and reflections. Lit presents the actual illumination without extra light-range circles. Fog uses scene lighting in Lit and its own colour under neutral illumination in Unlit. Fog and panorama switches do not create effects that the map has not configured. See the map guide for [fog settings](<../Editor User Guide/Tilesets Autotiles and Maps.md#fog>) and [panorama authoring](<../Editor User Guide/Tilesets Autotiles and Maps.md#map-list-and-composite-worlds>).

All six options are shared within the project and saved immediately in `Main.proj` under `editor.mapViewport`. Switching maps retains them, and reopening the project restores them. They do not change map data or create Undo steps. A project without saved options uses the defaults above.

## Actor and layer assistance

In Actor mode on an ordinary map or independently opened world child, every selected Actor has a yellow outline with a black backdrop above the map rendering. Single and multiple selection use the same marker. The outline follows translation, rotation and scale; Actors with missing textures or degenerate dimensions use a grid-cell marker. Selection markers remain visible when **Show Actor Origins** is off. Selection and batch editing gestures are described in [Selecting and editing Actors](<../Editor User Guide/Tilesets Autotiles and Maps.md#selecting-and-editing-actors>).

**Show Light Bounds** draws read-only ranges with a black outline, white dashes, a faint light-coloured fill and a contrasting centre marker. It does not change Actor selection or editing gestures. Lights on Actors without a texture are also previewed. Actor scaling changes the emitter position, while the circular range keeps its `lightRadius` in map pixels and scales only with the editor's zoom. Configure the emitter and its occlusion through [Light Component](<../Editor User Guide/Actor Components/Light.md>) and the map guide's [Lights and actors](<../Editor User Guide/Tilesets Autotiles and Maps.md#lights-and-actors>).

Ordinary maps and world previews draw an Actor whose resolved `visible` value is `false` at 25% opacity, including inherited defaults and instance overrides. Texture alpha, material opacity and the 50% dimming of non-current layers on an ordinary map still multiply this opacity. Selection outlines and error markers stay clear, and the Actor remains selectable and movable. These invisible Actors contribute neither lighting nor light occlusion. Hidden layers remain invisible, while placement ghosts and missing-texture placeholders retain their normal appearance. This is an editor preview aid; invisible Actors remain hidden at runtime.

Layer dimming preserves the authored layer order and does not reduce real light or occlusion strength. Selection markers, Actor origins and light ranges are editor overlays and do not appear in thumbnails.

## Worlds and thumbnails

The canvas and thumbnails appear as child maps load. A missing or unreadable child does not prevent the others from appearing. Editing maps or changing images updates their previews. Only children needed for the visible area and its lighting are loaded for the canvas. World rendering draws across regions by layer and evaluates cross-map lighting and occlusion; the selected placement retains its priority within each layer.

Child-map thumbnails use a single-region scene at time zero and follow View Mode, Show Fog and Show Panorama. Their caching and background scheduling are described in [Scheduling and frame ownership](<Architecture and Data Flow.md#scheduling-and-frame-ownership>).

World fog and region fog use the same world coordinates and time. Global fog is composited first, followed by region fog in stable placement order; adjacent regions with the same settings have a continuous pattern. A world uses its global panorama, while a child's panorama is used when that map is opened independently. The existing Cover scaling and camera scrolling apply to panorama previews.

## Refresh and Live Debug

The native viewport refreshes at up to 30 FPS at the same output resolution during dragging, and displays the final state after release. Hidden views stop refreshing. See [Scheduling and frame ownership](<Architecture and Data Flow.md#scheduling-and-frame-ownership>) for request handling and frame publication.

During **Individual Window** Play with **Live Debug**, the native viewport reflects live Actors' appearance, transforms, materials and lights, together with the map's fog, panorama and ambient light. World children retain world coordinates for these effects. Viewport options affect only the editor image; they do not alter the running game or grant editing permissions. Hidden Actors use the assistance rules above. See [Live Debug](<../Editor User Guide/Run Debug and Package.md#live-debug>) for the available operations and save boundary.

Embedded game execution hides the preview menu because that viewport displays the running game.
