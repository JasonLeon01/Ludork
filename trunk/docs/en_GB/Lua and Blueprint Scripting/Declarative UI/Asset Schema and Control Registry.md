# UI Asset Schema

Declarative UI assets are JSON documents under `Data/UI` that define control hierarchies, layout Slots, animation timelines and static properties. This page specifies the asset schema, resource key rules and per-control JSON serialisation contract.

## Asset key and structure

A UI asset declares `type = "uiAsset"` with no schema-version field; unknown fields are rejected. Its sole identity is its exact-case path relative to `Data/UI`, using forward slashes and no extension (for example, `Title.json` has key `Title`, and `Parts/Shared/WindowChrome.json` has key `Parts/Shared/WindowChrome`).

- **Key restrictions**: Keys cannot be absolute and cannot contain backslashes, empty segments, `.`, `..`, file extensions or repeated prefixes (`Assets/`, `UI/`, `Data/UI/`). Every path segment must omit dots so the generated Lua module has an unambiguous `require` path. Spaces, hyphens and Unicode names are supported.
- **Top-level fields**: The root document contains `designSize`, `palette` and one `root` node, with an optional `animations` array. Neither assets nor nodes store UUIDs.
- **System control IDs**: System controls use the bound class's canonical runtime path (such as `Engine.Canvas` or `Engine.FunctionalPlainText`); any other value is rejected as an unknown control.
- **Nested project assets**: Nested assets act as black boxes — the parent node declares only the project `controlId`, `name` and parent `slot`, with no internal overrides or `children`. The referenced asset must set `palette.exposed = true`, and direct or indirect nesting cycles are invalid.

Each node has the following schema:

| Field | Contract |
|---|---|
| `name` | Asset-local node identity, generated reference key and native lookup name; unique within the asset. Full identity is `(assetKey, name)`. |
| `controlId` | Canonical module-root native class path for a system control, or `Project:<relative-key>`. |
| `properties` | Static values owned by the control schema. |
| `slot` | Layout properties owned by the parent; omitted on the root node. |
| `editor` | Design-only values such as `previewText`. |
| `children` | Ordered child nodes; list order breaks ties when z-orders are equal. |

## Animation timelines

An asset may include an `animations` array defining linear keyframe timelines:

- **Entry fields**: Each animation specifies a non-empty `name` (unique per target), a required `target` (`null` for a Global animation, or one asset-local node name), a positive `duration`, an optional normalised `pivot` (default `[0.5, 0.5]`) and a `tracks` object. A target may reference a direct nested-asset node, never a node inside that nested asset.
- **Tracks and keyframes**: `tracks` accepts `translation`, `rotation`, `scale` and `colour`. Each present track is a non-empty array of `{ "time": number, "value": ... }` diamond keyframes with finite, strictly increasing times in `0..duration`. Vectors use two finite numbers (`scale` must be non-negative), and `colour` uses four integer RGBA channels in `0..255` interpolated independently. Missing tracks use identity; single-key tracks stay constant; times outside the keyed range hold the first or last value; interpolation is linear.
- **Transform composition**: `translation` adds a local post-layout offset, `rotation` adds a local angle increment, `scale` multiplies authored or layout scale, and `colour` multiplies the subtree's authored RGBA channels. Transforms compose around the target's local bounds and pivot without modifying layout position, rotation, scale, origin or Slot data, preventing offset accumulation on reflow. Rendering, absolute bounds and hit testing share this composed transform, and `Canvas` applies subtree colour once at its composition boundary.
- **Nesting and editor maintenance**: A nested asset exports only its Global animations; a same-name parent animation targeting that nested node overrides that instance (see [Animation playback](<Runtime Contract.md#animation-playback>)). In the editor, renaming a node updates its animation targets, deleting a node removes animations targeting its subtree, and duplicating a node copies and retargets its animations.

## Slots and resource keys

A parent's adapter determines child policy and Slot type: a Canvas parent supplies anchors, offsets, alignment, auto-size and z-order, whereas a List parent supplies list placement and rejects Canvas Slot fields.

- **Canvas Slot bounds and text alignment**: Canvas Slots position a control's local bounds, including any non-zero local bounds offset. Text `lineAlignment` aligns lines within the text bounds, while Slot `alignment` positions those bounds inside the parent — allowing right-aligned text in a right-aligned, auto-sized Slot to grow leftwards without shifting its right edge.
- **`Engine.ScrollBox`**: A multi-child Layout control using List Slots with serialised `size` and `windowSkin`. The native control owns scrolling, indicators and clipping.
- **`Engine.WrapBox`**: A single-template Layout control using List Slots with serialised `size` (available layout bounds), `count` (non-negative integer, default `1`) and `spacing` (two-number vector, default `[0, 0]`). Its sole child uses `slot: {}` and may own a subtree. Instances flow left to right and wrap by width; negative spacing is valid only when placement steps stay positive. An empty template or `count = 0` produces no repeated controls. Node names stay asset-local and unique, while generated View references stop at the WrapBox boundary — access runtime instances through its 1-based `get(i)` API.

Resource references follow strict canonical formats:

- **Asset files**: Exact-case logical paths starting with `/Game/Assets/`, using forward slashes and file extensions.
- **`textConfig`**: Extensionless path relative to `Data/TextConfigs`, with no directory prefix.
- **`opacityCurve`**: Extensionless scalar-curve path relative to `Data/Curves`, with no directory prefix.
- **Rejected forms**: Native absolute paths, `Assets/...`, category-relative paths, backslashes, traversal segments and alias prefixes are invalid.

Plain-text controls store font, character size, style, spacing, alignment, outline, glow and gradient directly on the node. `textConfig` is optional for shared or specialised text styles; when non-empty, the editor disables inline style fields and the runtime uses the TextConfig. Rich text may also reference a TextConfig for named tag styles.

## Control serialisation contract

This section defines the JSON field contract and required runtime types for specialised controls. For general control purposes, child policies and full Inspector property sets, see [UI Controls in the Palette](<../../Editor User Guide/UI Controls in the Palette.md>). Only assets with `palette.exposed = true` appear as project controls.

| Control | Serialised fields and defaults | Notes and runtime API |
|---|---|---|
| `Engine.ListView` | `childScrollingEnabled` (bool, default `false`). | Also assignable at runtime. See [ListView](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#listview>). |
| `Engine.Window` | `size`, `windowSkin`, `repeated`, `colour` (RGBA, default `[255, 255, 255, 255]`). | Lower `colour` alpha in JSON for translucent frames. |
| `Engine.Image`, `Engine.FunctionalImage` | `drawAs`: `"Image"` (default) or `"Tile"`. | Runtime setters require `Engine.ImageDrawAs`. See [Image](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#image>). |
| `Engine.EmitterView` | `particle` (extensionless key under `Data/Particles`, default `""`), `size` (`[100, 100]`), `anchor` (`[0.5, 0.5]`), `autoPlay` (`true`). | Accepts no children; uses parent Slot, ordering and clipping. GPU particles run in the enclosing Canvas's logical domain; `getEmitter()` exposes playback. Hidden/unmounted views freeze, disposal releases playback and switching Canvas resets the coordinate domain. See [Common Particles](<../../Editor User Guide/Common Particles.md>). |
| `Engine.Button` | `texture`, optional `textureRect`, `colour`, `hoverColour`, `pressedColour`, `gamepadButton` (logical name such as `"X"`, default `""`), `gamepadLongPress` (`false`). | See [Button](<../Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#button>). |
| `Engine.ProgressBar` | `size`, `progress`, `backgroundColor`, `fillColor`, `backgroundTexture` (`""`), `fillTexture` (`""`), optional `backgroundTextureRect`, `fillTextureRect`. | Empty texture draws a solid colour; non-empty values use `/Game/Assets/...` paths. Optional rects use `[x, y, width, height]` in source pixels (`null`/omitted uses the full image). Background fills the control; progress crops the fill left to right. Colours tint their layer (`[255, 255, 255, 255]` preserves image colours). See [ProgressBar](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#progressbar>). |
| `Engine.TextBox` | `size`, `windowSkin`, `text`, standard plain-text properties and editor-only `previewText`. | Accepts no children. See [TextBox](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#textbox>). |
| `Engine.TabView` | `size`, `windowSkin`, construction-only non-empty `items: string[]`, and inline text styles or optional `textConfig`. | Has no children or Slot. Before the first visible frame, the Controller supplies localised labels and binds selection (see [TabView](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#tabview>)). `selectedIndex` and key-hint tables are runtime-only; `tabCount` is rejected. |
| `Engine.CharacterView` | `size`, `texture`, optional `textureRect`, `characterScale`, `animatable`, `switchInterval`, `material` (`""` or `Data/Materials/...json`), `materialParameters` (string-keyed pure-data dictionary), `colour`. | Accepts no children. Owns cropping, animation, Actor-scale fitting, material drawing and fixed bounds. See [CharacterView](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#characterview>). |
