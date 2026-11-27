# UI Controls in the Palette

Browse every built-in control in the UI Asset Editor Palette, including its child and Slot rules, serialised properties and runtime API reference.

## How to read this page

Entries are grouped by Palette category and titled with the exact `controlId` written to UI asset JSON. Tables show each control's child policy, Slot type and control-specific serialised property keys (the Inspector displays British English labels such as **Colour**, **Fill Colour**, **Outline Colour**, **Glow Colour** and **Background Colour**).

### Children and Slots

`childPolicy` and `slotType` determine what children a container accepts and how they are positioned:

| `childPolicy` | `slotType` | Controls | Child layout |
|---|---|---|---|
| `multiple` | `canvas` | `Canvas` | Anchors, offsets, alignment, auto-size and z-order |
| `multiple` | `list` | `ListView`, `ScrollBox` | List placement managed by the parent |
| `single` | `list` | `WrapBox` | List placement for a single template child |
| `none` | — | All other 19 controls | Accepts no children |

Canvas Slots position a child's local bounds; List Slots reject Canvas Slot fields. See [Slots and resource keys](<../Lua and Blueprint Scripting/Declarative UI/Asset Schema and Control Registry.md#slots-and-resource-keys>).

### Properties every control shares

All 23 built-in controls share `visible`, `rotation`, `scale` and `origin`, omitted from the entry tables below. Six text-bearing controls (`PlainText`, `RichText`, `FunctionalPlainText`, `FunctionalRichText`, `TextBox` and `DropBox`) also store an editor-only `previewText` inside `editor` so layouts stay legible in the Designer before a Controller assigns runtime text.

### The inline text-style group

Seven controls (`CheckBox`, `DropBox`, `FunctionalPlainText`, `GamepadHintBar`, `PlainText`, `TabView` and `TextBox`) share the same inline text-style properties: `textConfig`, `font`, `characterSize`, `bold`, `italic`, `underlined`, `strikeThrough`, `slantAngle`, `fillColor`, `letterSpacing`, `lineSpacing`, `lineAlignment`, `outlineColor`, `outlineThickness`, the four `glow*` fields and the three `gradient*` fields.

Entry tables abbreviate this group as *text style*. When `textConfig` is non-empty, inline fields are disabled in the editor and ignored in favour of the TextConfig at runtime.

`RichText` and `FunctionalRichText` accept `textConfig` only and style named tags through that TextConfig.

### System controls and project assets

The 23 controls below are built in. Project UI assets with **Exposed** enabled (`palette.exposed = true`) also appear in the Palette as reusable units; edit their internals in the source asset.

## Layout controls

Layout controls organise child nodes and are the only controls that accept children.

### Engine.Canvas

Free-positioning container and the standard root for a new screen. Children position themselves via Canvas Slots rather than flowing automatically.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Layout | any number | `canvas` | `size` |

Defines the coordinate space and draw order of its contents. API: [Canvas](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#canvas>).

### Engine.ListView

Multi-child grid/list container that arranges children into rows and columns and draws only visible entries.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Layout | any number | `list` | `size`, `columns`, `defaultItemHeight`, `fixItemHeight`, `childScrollingEnabled` |

Defaults: `columns = 1`, `defaultItemHeight = 32`, `childScrollingEnabled = false` (enable in Inspector to scroll wide children within columns). `fixItemHeight` enforces a uniform row height. API: [ListView](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#listview>).

### Engine.ScrollBox

Clipped scrollable viewport whose native control owns scrolling, clipping and window-skin overflow indicators.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Layout | any number | `list` | `size`, `windowSkin` |

An empty `windowSkin` loads the current system window skin. API: [ScrollBox](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#scrollbox>).

### Engine.WrapBox

Repeats one authored child template at equal spacing — ideal for status icons, grids and cell backgrounds.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Layout | one template | `list` | `size`, `count`, `spacing` |

Items flow left to right and wrap at the available width. See [UI Asset Editor](<UI Asset Editor.md>) for template editing and [WrapBox](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#wrapbox>) for runtime access.

## Visual controls

Visual controls render content without accepting focus or input.

### Engine.CharacterView

Renders frame-animated character or enemy sprite sheets inside fixed bounds.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `size`, `texture`, `textureRect`, `characterScale`, `animatable`, `switchInterval`, `material`, `materialParameters`, `colour` |

`textureRect` picks the initial frame, `characterScale` scales it, and `animatable` + positive `switchInterval` cycles equal-width horizontal frames.

`material` accepts a full `Data/Materials/...json` path (empty uses default) with per-instance overrides in `materialParameters`, cleared on material change and edited via the material's Inspector controls. API: [CharacterView](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#characterview>).

### Engine.EmitterView

Displays a GPU particle emitter inside a UI layout.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `particle`, `size`, `anchor`, `autoPlay` |

`particle` is an extensionless key under `Data/Particles`. See [Common Particles](<Common Particles.md>) for authoring and [EmitterView](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Time Rendering and Particles.md#emitterview>) for runtime playback.

### Engine.Image

Static picture rendered stretched or tiled.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `drawAs`, `texture`, `textureRect`, `colour` |

Set **Draw As** in the Inspector to **Image** (stretch) or **Tile** (repeat `textureRect` across the layout area, cropping the final partial tile); global UI Scale applies to the combined output. API: [Image](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#image>).

### Engine.ProgressBar

Background layer paired with a fill revealed from left to right.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `size`, `progress`, `backgroundTexture`, `fillTexture`, `backgroundTextureRect`, `fillTextureRect`, `backgroundColor`, `fillColor` |

`progress` clamps to `[0, 1]` (non-finite values become `0`), cropping the fill rather than squashing it. Empty textures draw solid-colour layers; non-empty values use canonical `/Game/Assets/...` paths. Each colour tints its layer (`[255, 255, 255, 255]` leaves textures untinted). API: [ProgressBar](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#progressbar>).

### Engine.Rect

Skinned rectangle drawn from a window skin for panels, frames and selection highlights.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `size`, `windowSkin`, `opacityCurve` |

`opacityCurve` is an extensionless scalar curve key relative to `Data/Curves`. API: [Rect](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#rect>).

### Engine.SolidRect

Solid-colour rectangle with an optional outline.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `size`, `fillColor`, `outlineColor`, `outlineThickness` |

Defaults to `outlineThickness = 0` and transparent `outlineColor`, drawing a plain fill until configured. API: [SolidRect](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#solidrect>).

### Engine.Window

Skinned 9-slice window frame — the standard backdrop for dialogs, menus and message boxes.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `size`, `windowSkin`, `repeated`, `colour` |

`repeated` tiles (`true`) or stretches (`false`) the background while borders always tile between corners. `colour` defaults to `[255, 255, 255, 255]`; lower its alpha for a translucent frame. API: [Window](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Text and Windows.md#window>).

## Input controls

Input controls derive from `FunctionalBase` and respond to focus and player input only while both the control and every `FunctionalBase` ancestor are active and every `ControlBase` ancestor is visible.

### Engine.Button

Clickable button with hover/pressed tints and optional gamepad prompt binding.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `texture`, `textureRect`, `colour`, `hoverColour`, `pressedColour`, `gamepadButton`, `gamepadLongPress` |

An empty `texture` falls back to the active window skin's selection-Rect image; backgrounds stay visible across all input devices. Buttons take no children — place labels as separate text controls.

When `Engine.Input.getInputHintDevice()` selects Gamepad, the bound `gamepadButton` glyph (logical name such as `"X"`, default `""`; `gamepadLongPress` defaults to `false`) renders without altering size, bounds, hit area or sibling text controls. API: [Button](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#button>).

### Engine.CheckBox

Two-state toggle that renders its own label and notifies state changes.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, `checked`, `windowSkin`, *text style* |

Call `toggle()` from code and bind `setOnCheckedChanged` for updates. API: [CheckBox](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#checkbox>).

### Engine.DropBox

Collapsed single-choice selector that expands into a popup list.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, `windowSkin`, *text style* |

Populate choices at runtime from the Controller via `setItems` (options are not serialised in the asset). API: [DropBox](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#dropbox>).

### Engine.FunctionalImage

Focusable, clickable `Image` and the base class of `CharacterView`.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `drawAs`, `texture`, `textureRect`, `colour` |

Shares `Image` serialisation and `drawAs` behaviour; use `Engine.Image` when click/focus input is unneeded. API: [FunctionalImage](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#functionalimage>).

### Engine.FunctionalPlainText

Focusable, clickable `PlainText`.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `text`, `colour`, *text style* |

API: [FunctionalPlainText](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#functionalplaintext>).

### Engine.FunctionalRichText

Focusable, clickable `RichText`.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `textConfig`, `text`, `colour` |

API: [FunctionalRichText](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#functionalrichtext>).

### Engine.GamepadHintBar

Displays up to three centred `Engine.GamepadHint` entries supplied by the Controller while a gamepad is connected.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, *text style* |

API: [GamepadHintBar](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#gamepadhintbar>).

### Engine.Slider

Draggable integer range input supporting pointer, keyboard and touch editing.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, `minValue`, `maxValue`, `value`, `lineTexture`, `handleTexture` |

Defaults: `minValue = 0`, `maxValue = 100`, `lineTexture = "/Game/Assets/System/SliderLine.png"`, `handleTexture = "/Game/Assets/System/SliderHandle.png"`. Bind `setOnValueChanged` for updates. API: [Slider](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#slider>).

### Engine.TabView

Horizontal row of selectable tabs.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, `windowSkin`, `items`, *text style* |

`items` is a construction-only non-empty `string[]`; the Controller supplies localised labels and selection handlers before the first visible frame. `selectedIndex` and key-hint tables exist only at runtime, and `tabCount` is not an asset property.

See [Control serialisation contract](<../Lua and Blueprint Scripting/Declarative UI/Asset Schema and Control Registry.md#control-serialisation-contract>) and [TabView](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#tabview>).

### Engine.TextBox

Editable single-field text input.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, `windowSkin`, `text`, *text style* |

`text` sets the initial runtime value; `previewText` appears in the Designer only. API: [TextBox](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#textbox>).

## Text controls

Text controls render text without accepting focus or input (use the Functional variants when text must be interactive).

### Engine.PlainText

Static text styled directly on the node or via a TextConfig.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Text | none | — | `text`, `colour`, *text style* |

API: [PlainText](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Text and Windows.md#plaintext>).

### Engine.RichText

Static text using named rich-text tags resolved through a TextConfig.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Text | none | — | `textConfig`, `text`, `colour` |

API: [RichText](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Text and Windows.md#richtext>).

## Runtime-only types

`Engine.SectorShape` and `Engine.AssetInstance` are available in code only and do not appear as built-in Palette entries (see [Interactive and Declarative UI Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md>)). Exposed project UI assets appear in the Palette under their own names, and their generated Views expose the underlying runtime asset on `.instance`.
