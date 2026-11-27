# Engine: Layout and Image Controls

This page documents image, character-preview, layout container and rectangle controls under `Engine`: `Image`, `CharacterView`, `ListView`, `WrapBox`, `ScrollBox`, `Rect`, `SolidRect` and `SpriteBase`. Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin.

## Image

**Inherits:** `Engine.SpriteBase`

Displays a stretched or tiled image (`FunctionalImage` inherits this API).

- **`drawAs` mode:** Uses the bound `Engine.ImageDrawAs` enum (`Image` or `Tile`, defaulting to `Engine.ImageDrawAs.Image`) via `getDrawAs()`, `setDrawAs(mode)` or the `drawAs` property. Lua must pass enum members (such as `image:setDrawAs(Engine.ImageDrawAs.Tile)`) rather than numbers or strings; declarative JSON serialises `"Image"` or `"Tile"` and decodes to the enum value.
- **Scaling and tiling:** `Image` stretches the picture as local scale grows, whereas `Tile` repeats source-sized tiles via SFML texture repetition and crops partial edge tiles. Global UI Scale scales the final result in both modes.
- **Private repeating texture:** Each `Image` owns its repeating texture so switching modes does not affect other controls sharing the source texture. An optional `textureRect` defines the tile and refreshes that private texture whenever the texture or rectangle changes.

## CharacterView

**Inherits:** `Engine.FunctionalImage`

Renders frame-animated character and enemy sheets inside fixed `size` bounds through the 2D material pipeline.

- **Framing and animation:** `textureRect` selects the initial frame, `characterScale` scales it before centring and fitting without enlargement, and a positive `switchInterval` advances equal-width horizontal frames while `animatable` is `true` (non-positive pauses animation). `setFrameRect` updates the reset frame; changing declarative `texture` resets it to the full texture. An invisible `CharacterView` skips rendering, including direct draw calls.
- **Materials and runtime data:** `material` is empty or a complete `Data/Materials/...json` path, and `materialParameters` holds per-control overrides (never shared with Actors). Apply a new material before applying its parameters. Asset properties (`size`, `texture`, optional `textureRect`, `characterScale`, `animatable`, `switchInterval`, `material`, `materialParameters`, `colour`) provide editor preview values; runtime code passes the live Actor resource and parameter snapshot to `setCharacter`.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setCharacter` | `texture: sf.Texture`, `frameRect: sf.IntRect`, `characterScale: sf.Vector2f`, `animatable: bool`, `switchInterval: float`, `material: string = ""`, `materialParameters: Dict[string, any] = {}` | — | — |
| `setCharacterTexture` | `texture: sf.Texture` | — | — |
| `getCharacterScale` | — | `sf.Vector2f` | Pure. |
| `setCharacterScale` | `scale: sf.Vector2f` | — | — |
| `getAnimatable` | — | `bool` | Pure. |
| `setAnimatable` | `animatable: bool` | — | — |
| `getSwitchInterval` | — | `float` | Pure. |
| `setSwitchInterval` | `switchInterval: float` | — | — |
| `getSwitchTimer` | — | `float` | Pure. |
| `getFrameRect` | — | `sf.IntRect` | Pure. |
| `setFrameRect` | `frameRect: sf.IntRect` | — | — |
| `resetFrameRectToTexture` | — | — | — |
| `getMaterial` | — | `string` | Pure. |
| `setMaterial` | `material: string` | — | — |
| `getMaterialParameters` | — | `Dict[string, any]` | Pure. |
| `setMaterialParameters` | `parameters: Dict[string, any]` | — | — |
| `resetAnimation` | — | — | — |
| `getSize` | — | `sf.Vector2f` | Pure. |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `getGlobalBounds` | — | `sf.FloatRect` | Pure. |
| `resize` | `size: sf.Vector2f` | — | — |
| `update` | `deltaTime: float` | — | — |

## ListView

**Inherits:** `Engine.ControlBase`, `Engine.FunctionalBase`

Multi-column vertical layout container with optional horizontal marquee scrolling for wide children. An invisible `ListView` skips drawing and layout preparation during draw; visible lists draw only visible children. Each update phase (`update`, `lateUpdate`, `fixedUpdate`) updates visible functional children before running the list's own tick hook.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getOrigin` | — | `sf.Vector2f` | Pure. |
| `setOrigin` | `origin: sf.Vector2f` | — | — |
| `getColumns` | — | `int` | Pure. |
| `getChildScrollingEnabled` | — | `bool` | Pure. |
| `setChildScrollingEnabled` | `enabled: bool` | — | — |
| `getDefaultItemSize` | — | `sf.Vector2f` | Pure. |
| `getItemLayoutRect` | `index: int` | `sf.FloatRect` | Pure. |
| `getSize` | — | `sf.Vector2f` | Pure. |
| `setSize` | `size: sf.Vector2i` | — | — |
| `setColumns` | `columns: int` | — | — |
| `getChildren` | — | `Engine.ControlBase[]` | Pure. |
| `addChild` | `child: Engine.ControlBase` | — | — |
| `removeChild` | `child: Engine.ControlBase` | — | — |
| `clearChildren` | — | — | — |
| `getRenderStates` | — | `sf.RenderStates` | Pure. |
| `update` | `deltaTime: float` | — | — |
| `lateUpdate` | `deltaTime: float` | — | — |
| `fixedUpdate` | `fixedDelta: float` | — | — |
| `invalidatePositions` | — | — | — |
| `applyPositions` | — | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

Layout queries and child-scrolling behaviour:

- **Layout queries:** `getDefaultItemSize()` returns the native column width and authored default row height in logical pixels. `getItemLayoutRect(index)` returns the zero-based cell in list-local coordinates (column width and fixed height or tallest child height), settling pending layout and rejecting out-of-range indices. Use these values for dynamic row sizes, selection and hit testing rather than reimplementing list spacing in Lua.
- **Content bounds:** With child scrolling disabled, `getContentBounds()` returns the laid-out union of visible children rather than only the declared size, so an enclosing `ScrollBox` scrolls only when laid-out extent strictly exceeds the viewport.
- **Horizontal child scrolling:** `childScrollingEnabled` defaults to `false` (enable in the Inspector or via `setChildScrollingEnabled(true)`). Any visible child whose scaled local width exceeds `(size.x - 32) / columns` starts left-aligned, pauses `1` s, scrolls left at `30` logical pixels/s until its right edge is visible, pauses `1` s and resets (fitting children stay centred, respecting origin and scale; speed and pause are fixed native constants).
- **Scrolling lifecycle:** Only `update(deltaTime)` advances scrolling; hiding the child, list or an ancestor pauses it. Local bounds, origin, scale or column-width changes restart motion when measured (or after `invalidatePositions()` and `applyPositions()`); disabling restores centred placement immediately and removing a child discards its scroll state.
- **Clipping and bounds during scrolling:** While enabled, child drawing and descendant hit areas clip horizontally to their column (popup overlays keep their normal behaviour), item selection cells stay fixed, and `getContentBounds()` reports the declared width horizontally while keeping laid-out vertical heights.

## WrapBox

Repeats a single authored child template (including its entire subtree) left to right, wrapping when the next item exceeds the available width. Native asset instantiation constructs independent instances without Lua cloning.

Declarative properties are `size` (default `[100, 100]`), `count` (default `1`) and `spacing` (default `[0, 0]`), with at most one template in an empty List Slot. Cell dimensions come from the laid-out template; negative spacing allows overlap provided horizontal and vertical placement steps remain positive. Reflow preserves existing instances and state.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | — |
| `setSize` | `size: sf.Vector2f` | — | — |
| `getCount` | — | `int` | — |
| `setCount` | `count: int` | — | Non-negative count; increasing appends instances and decreasing releases only the removed tail. |
| `getSpacing` | — | `sf.Vector2f` | — |
| `setSpacing` | `spacing: sf.Vector2f` | — | — |
| `get` | `i: int` | `Engine.ControlBase` | 1-based index; out-of-range raises an error. |

Count `0` or an absent template creates no instances. `WrapBox` owns repeated controls' update, input and release lifetimes. Generated Views expose the `WrapBox` itself; template descendants live inside each instance boundary and are accessed via `get(i)` and that control's child APIs. The editor previews all repetitions while edits modify the single authored template.

## ScrollBox

**Inherits:** `Engine.Canvas`

Clipped multi-child viewport supporting wheel and touch scrolling with window-skin overflow indicators. Declarative assets configure `size` and `windowSkin` (an empty `windowSkin` loads the active system window skin). Inherits all `Engine.Canvas` child methods.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | Pure. |
| `resize` | `size: sf.Vector2f` | — | — |
| `setWindowSkin` | `windowSkin: sf.Image` | — | Rebuilds overflow indicators immediately. |
| `getScrollOffset` | — | `sf.Vector2f` | Pure. |
| `setScrollOffset` | `offset: sf.Vector2f` | — | — |
| `getMaxScrollOffset` | — | `sf.Vector2f` | Pure. |
| `getScrollingEnabled` | — | `bool` | Pure. |
| `setScrollingEnabled` | `enabled: bool` | — | — |
| `scrollDescendantIntoView` | `descendant: Engine.ControlBase` | — | Rejects controls outside this `ScrollBox`. |

Each axis derives from visible direct-child content bounds and activates only when those bounds exceed the viewport (otherwise offset and maximum are zero). Offsets clamp after writes, resizes and content shrinks; rendering and descendant hit testing clip to the viewport.

Ordinary wheel ticks scroll by `32` logical pixels, while precise wheel deltas apply directly per axis. Touch drag begins after the input threshold with no inertia; selectable-list taps, Config Slider direction ownership and DropBox popup input stay unchanged.

### Window-skin overflow indicators

The system window skin must contain the complete `32×32` indicator atlas at `(144, 16)` (a smaller skin raises an error). `ScrollBox` ignores the centre `(156, 28, 8, 8)` and four corners, sampling these nearest-neighbour regions:

| Direction | Source rectangle | Viewport position | Visible when |
|---|---|---|---|
| Up | `(156, 16, 8, 12)` | Centred on top edge | `offset.y > 0` |
| Down | `(156, 36, 8, 12)` | Centred on bottom edge | `offset.y < maximum.y` |
| Left | `(144, 28, 12, 8)` | Centred on left edge | `offset.x > 0` |
| Right | `(164, 28, 12, 8)` | Centred on right edge | `offset.x < maximum.x` |

Indicators scale with global UI `Scale`, use nearest-neighbour sampling, draw above content and do not affect scrolling, bounds or hit testing.

## Rect

**Inherits:** `Engine.SpriteBase`

Window-skin-styled selection or highlight rectangle with adjustable opacity multiplier.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setOpacityMultiplier` | `multiplier: float` | — | — |
| `resize` | `size: sf.Vector2f` | — | — |
| `setWindowSkin` | `windowSkin: sf.Image` | — | — |
| `getSize` | — | `sf.Vector2u` | Pure. |
| `update` | `deltaTime: float` | — | — |

## SolidRect

**Inherits:** `Engine.ControlBase`

Solid-colour rectangle control with configurable fill colour, outline colour and outline thickness.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | Pure. |
| `setSize` | `size: sf.Vector2f` | — | — |
| `getFillColor` | — | `sf.Color` | Pure. |
| `setFillColor` | `color: sf.Color` | — | — |
| `getOutlineColor` | — | `sf.Color` | Pure. |
| `setOutlineColor` | `color: sf.Color` | — | — |
| `getOutlineThickness` | — | `float` | Pure. |
| `setOutlineThickness` | `thickness: float` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `getGlobalBounds` | — | `sf.FloatRect` | Pure. |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## SpriteBase

**Inherits:** `Engine.ControlBase`

Base class for textured UI controls exposing texture, source rectangle and tint colour.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setTexture` | `texture: sf.Texture`, `resetRect: bool` | — | — |
| `getTexture` | — | `sf.Texture` | Pure. |
| `setTextureRect` | `rect: sf.IntRect` | — | — |
| `getTextureRect` | — | `sf.IntRect` | Pure. |
| `setColour` | `colour: sf.Color` | — | — |
| `getColour` | — | `sf.Color` | Pure. |
| `getSize` | — | `sf.Vector2f` | Pure. |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `getGlobalBounds` | — | `sf.FloatRect` | Pure. |
| `getRenderStates` | — | `sf.RenderStates` | Pure. |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |
