# UI Asset Schema

## Asset key and structure

A UI asset uses `type = "uiAsset"` and follows the field contract below. It does not carry a schema-version field. Unknown fields are rejected. Its only asset identity is the exact-case path relative to `Data/UI`, with forward slashes and no extension. `Title.json` has key `Title`, and `Parts/Shared/WindowChrome.json` has key `Parts/Shared/WindowChrome`.

Keys cannot be absolute, contain backslashes, empty segments, `.` or `..`, include an extension, or repeat prefixes such as `Assets/`, `UI/` or `Data/UI/`. Every path segment must be free of dots, so its generated Lua module has an unambiguous require path. Spaces, hyphens and Unicode names remain supported. The asset object contains `designSize`, `palette` and one `root` node. Neither assets nor nodes contain UUID fields.

Each node contains:

| Field | Contract |
|---|---|
| `name` | Asset-local node identity, generated reference key and native lookup name; unique within the asset. Its full identity is `(assetKey, name)`. |
| `controlId` | Canonical module-root native class path for a system control, or `Project:<relative-key>`. |
| `properties` | Values owned by the control schema. |
| `slot` | Layout owned by the parent; absent on the root. |
| `editor` | Design-only values such as `previewText`. |
| `children` | Ordered child nodes; order is the stable tie-breaker for equal z-order. |

Nested project assets are black boxes. Their node declares the project `controlId`, `name` and parent Slot, with no internal overrides or children. The target must be exposed, and direct or indirect nesting cycles are invalid.

System IDs use the bound class's sole runtime path, for example `Engine.Canvas` or `Engine.FunctionalPlainText`. A system ID that does not match its bound class's sole runtime path is an unknown control.

## Animation timelines

The asset may contain an `animations` array. Each entry has a non-empty `name`, a required `target` (`null` for Global or one asset-local node name), a positive `duration`, an optional normalised `pivot` defaulting to `[0.5, 0.5]`, and a `tracks` object. Names are unique within one target. A target may be a direct nested-asset node, but never an internal node behind that boundary.

`tracks` accepts `translation`, `rotation`, `scale` and `colour`. Every present track is a non-empty array of diamond keys shaped as `{ "time": number, "value": ... }`. Times are finite, strictly increasing and within `0..duration`. Vector values use two finite numbers, and scale is non-negative. Colour values are four integer RGBA components within `0..255`, and each component is interpolated independently. Missing tracks use identity, and a track with one key stays constant. Outside a track's keyed range, the runtime holds the first or last value. Interpolation is linear.

Translation is a local additive offset after layout. Rotation is a local angle increment. Scale multiplies the authored or layout scale. Colour multiplies the subtree's authored RGBA components. Presentation transforms are composed around the target's local bounds and pivot without modifying position, rotation, scale, origin or Slot data, so reflow does not accumulate animation offsets. Rendering, absolute bounds and hit testing share that composed transform. Canvas applies the subtree colour once at its composition boundary.

A nested asset exports only its Global animations. Same-name parent animations targeted at that nested node override that instance; see [animation playback](<Runtime Contract.md#animation-playback>) for dispatch and completion rules.

Renaming a node rewrites its animation targets. Deleting a node removes animations that target the deleted subtree. Duplicating a node copies those animations and retargets them to the copy.

## Slots and resource keys

A Canvas parent supplies anchors, offsets, alignment, auto-size and z-order. A List parent supplies list placement and does not accept Canvas Slot fields. The parent's adapter declaration determines child policy and Slot type.

Canvas Slots position the control's local bounds, including their non-zero offset. Text `lineAlignment` controls line placement within the text, while Slot `alignment` places the resulting bounds in the parent. Right-aligned text can therefore use a right-aligned, auto-sized Slot without moving its right edge when the text width changes.

`Engine.ScrollBox` is a multi-child Layout control with List Slots and serialised `size` and `windowSkin`. The native control owns scrolling, indicators and clipping.

`Engine.WrapBox` is a single-template Layout control with List Slots. Its `size` sets the available layout bounds, `count` is a non-negative integer defaulting to `1`, and `spacing` is a two-number vector defaulting to `[0,0]`. Its sole child has `slot: {}` and may own a subtree. Native instances flow left to right and wrap by width; negative spacing is valid only with positive placement steps. An empty template or zero count produces no repeated controls. Authored node names remain asset-local and unique, but generated references stop at the WrapBox template boundary; use its 1-based `get(i)` API for runtime instances.

- File resources use exact-case logical paths beginning with `/Game/Assets/`, with forward slashes and extensions.
- `textConfig` is relative to `Data/TextConfigs`, with no directory prefix or extension.
- `opacityCurve` is relative to `Data/Curves`, with no directory prefix or extension, and resolves to a scalar curve.
- Native absolute paths, `Assets/...`, category-relative paths, backslashes, traversal and alias prefixes are invalid.

Plain-text controls store font, character size, style, spacing, alignment, outline, glow and gradient directly on the UI node. `textConfig` remains optional for shared or specialised text. When it is non-empty, the editor disables those inline style fields and the runtime uses the TextConfig. Rich text may use a TextConfig for named tag styles.

## Control serialisation contract

This section is the JSON field authority for the controls it names: what a node serialises, and which runtime type each value requires. For what a control is for, whether it accepts children, and its complete Inspector property set, see [UI Controls in the Palette](<../../Editor User Guide/UI Controls in the Palette.md>).

`Engine.ListView` accepts `childScrollingEnabled` as a JSON boolean, defaulting to `false`. It can also be assigned at runtime. See [ListView](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#listview>) for child scrolling, clipping and layout behaviour.

`Engine.Window` stores `size`, `windowSkin`, `repeated` and `colour` in the asset. `colour` is an RGBA tint, defaulting to `[255,255,255,255]`; set its alpha in JSON for translucent window frames.

`Engine.Image` and `Engine.FunctionalImage` serialise `drawAs` as `"Image"` (default) or `"Tile"`. Runtime calls require `Engine.ImageDrawAs`; see [Image](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#image>) for tiling and scaling behaviour.

`Engine.EmitterView` serialises `particle` (extensionless key under `Data/Particles`, default `""`), `size` (default `[100,100]`), `anchor` (default `[0.5,0.5]`) and `autoPlay` (default `true`), and accepts no children. It uses its parent's existing Slot, ordering and clipping. Its GPU particles use the containing Canvas's logical domain, and `getEmitter()` exposes playback to code. Hidden/unmounted views freeze; disposal releases playback; switching Canvas resets the coordinate domain. See [Common Particles](<../../Editor User Guide/Common Particles.md>) for resource schema, timing and scaling.

`Engine.Button` accepts `texture`, optional `textureRect`, `colour`, `hoverColour`, `pressedColour`, `gamepadButton` and `gamepadLongPress`. `gamepadButton` is a logical name such as `"X"` and defaults to `""`; `gamepadLongPress` defaults to `false`. Runtime bindings and interaction are described in [Engine: Runtime Values and Functional Controls](<../Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#button>).

`Engine.ProgressBar` accepts `size`, `progress`, `backgroundColor`, `fillColor`, `backgroundTexture`, `fillTexture`, `backgroundTextureRect` and `fillTextureRect`. Each texture defaults to `""` for a solid-colour layer; non-empty values use canonical `/Game/Assets/...` paths and the editor's image picker. Each optional Rect uses `[x,y,width,height]` in source pixels; omission or `null` selects the whole image. The background fills the control, while progress reveals the fill from left to right by cropping its image. Colours tint their respective layers; use `[255,255,255,255]` to preserve image colours. Runtime APIs are described in [Engine: Interactive and Declarative UI Controls](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#progressbar>).

`Engine.TextBox` serialises `size`, `windowSkin`, `text`, the standard plain-text properties and editor-only `previewText`, and accepts no children. Editing and callbacks are described in [Engine: Interactive and Declarative UI Controls](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#textbox>).

Only assets with `palette.exposed = true` appear as project controls.

`Engine.TabView` serialises `size`, `windowSkin`, the construction-only non-empty `items: string[]` and the same inline text style or optional `textConfig` as plain text, and has no children or Slot of its own. Before the first visible frame, the Controller supplies localised labels and binds selection behaviour through [Engine: Interactive and Declarative UI Controls](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#tabview>). `selectedIndex` and key-hint tables are runtime-only, and `tabCount` is not accepted.

`Engine.CharacterView` serialises `size`, `texture`, optional `textureRect`, `characterScale`, `animatable`, `switchInterval`, `material`, `materialParameters` and `colour`, and accepts no children. `material` is empty or a complete `Data/Materials/...json` resource path; `materialParameters` is a string-keyed pure-data override dictionary. The control owns cropping, animation, actor-scale fitting, material drawing and fixed bounds.

## Related pages

- [UI Asset Editor](<../../Editor User Guide/UI Asset Editor.md>)
- [Native UI Adapters](<../../Native C++ Development/Native UI Adapters.md>)
