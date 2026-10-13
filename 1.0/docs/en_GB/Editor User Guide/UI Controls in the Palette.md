# UI Controls in the Palette

The Palette lists everything you can drag into a UI asset: the engine's built-in controls, followed by the project's own exposed assets. This page covers the built-in controls — what each one is for, whether it accepts children, which Slot its children use, and which properties matter first. Runtime APIs are linked from each entry instead of being repeated here.

## How to read this page

Every entry is headed by the control's `controlId`, which is the exact value written into a node's `controlId` field. Entries are grouped by the Palette's own categories, and each carries one table:

| Column | Meaning |
|---|---|
| Category | The Palette group the control appears under. |
| Children | `childPolicy` — whether the control accepts child nodes, and how many. |
| Slot | `slotType` — the kind of Slot this control supplies to each child. |
| Key properties | The control's own serialised properties, excluding those every control shares. |

### Children and Slots

`childPolicy` and `slotType` define which children a container accepts and how they are laid out:

| childPolicy | slotType | Controls | What each child gets |
|---|---|---|---|
| `multiple` | `canvas` | Canvas | Anchors, offsets, alignment, auto-size and z-order |
| `multiple` | `list` | ListView, ScrollBox | List placement owned by the parent |
| `single` | `list` | WrapBox | List placement, for exactly one template child |
| `none` | — | The other 19 controls | Nothing; they accept no children |

A Canvas Slot positions the child's local bounds. A List Slot does not accept Canvas Slot fields. The full field contract is in [UI Asset Schema and Control Registry](<../Lua and Blueprint Scripting/Declarative UI/Asset Schema and Control Registry.md#slots-and-resource-keys>).

### Properties every control shares

`visible`, `rotation`, `scale` and `origin` are on all 23 controls and are left out of the entry tables.

Six text-bearing controls — PlainText, RichText, FunctionalPlainText, FunctionalRichText, TextBox and DropBox — also carry an editor-only `previewText`. The Designer shows it so the layout stays readable before a Controller supplies real strings. It lives in the node's `editor` object and is design-only.

### The inline text-style group

Seven controls carry the same inline text-style properties: `textConfig`, `font`, `characterSize`, `bold`, `italic`, `underlined`, `strikeThrough`, `slantAngle`, `fillColor`, `letterSpacing`, `lineSpacing`, `lineAlignment`, `outlineColor`, `outlineThickness`, the four `glow*` fields and the three `gradient*` fields. They are CheckBox, DropBox, FunctionalPlainText, GamepadHintBar, PlainText, TabView and TextBox. Their entries below say *text style* rather than repeating that list. When `textConfig` is non-empty, the editor disables the inline fields and the runtime uses the TextConfig.

RichText and FunctionalRichText take `textConfig` but none of the inline fields; they style named tags through the TextConfig.

### System controls and project assets

The 23 controls below are built in. The Palette also lists project UI assets with **Exposed** enabled (`palette.exposed = true`). Add these as reusable units; edit their internal contents in the original asset.

## Layout controls

Layout controls organise their children, and are the only controls that accept child nodes.

### Engine.Canvas

The free-positioning container, and the normal root of a new screen. Children are placed by their own Canvas Slots rather than flowed by the parent.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Layout | any number | canvas | `size` |

Canvas sets the coordinate space and stacking order of its contents. See [Engine: Runtime Values and Functional Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#canvas>).

### Engine.ListView

A multi-child list that arranges children into rows and columns, and draws only the ones currently visible.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Layout | any number | list | `size`, `columns`, `defaultItemHeight`, `fixItemHeight` |

`columns` defaults to 1 and `defaultItemHeight` to 32; `fixItemHeight` keeps every row at that height. See [Engine: Layout and Image Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#listview>).

### Engine.ScrollBox

A clipped, scrollable viewport for more content than fits on screen. The native control owns scrolling, the overflow indicators and the clipping.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Layout | any number | list | `size`, `windowSkin` |

An empty `windowSkin` loads the current system window skin. See [Engine: Layout and Image Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#scrollbox>), which also covers the window-skin overflow indicators.

### Engine.WrapBox

Repeats one authored child template at equal spacing — status flames, an icon grid, cell backgrounds.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Layout | one template | list | `size`, `count`, `spacing` |

Items flow left to right and wrap at the available width. Editing repetitions is described in [UI Asset Editor](<UI Asset Editor.md>); runtime access is in [Engine: Layout and Image Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#wrapbox>).

## Visual controls

Visual controls draw something and take no input. `ProgressBar` belongs to this group even though the value it displays changes at runtime.

### Engine.CharacterView

Renders frame-animated character and enemy sheets inside fixed bounds.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `size`, `texture`, `textureRect`, `characterScale`, `animatable`, `switchInterval`, `shader`, `hue`, `colour` |

`textureRect` selects the initial frame, and `characterScale` adjusts its size. Set `animatable` and a positive `switchInterval` to play equal-width horizontal frames. `shader` accepts a complete `/Game/Assets/Shaders/...` path or an empty string. See [Engine: Layout and Image Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#characterview>).

### Engine.EmitterView

Shows a GPU particle emitter inside the UI.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `particle`, `size`, `anchor`, `autoPlay` |

`particle` is an extensionless key under `Data/Particles`. Choose the effect and set its size, anchor and automatic playback. See [Common Particles](<Common Particles.md>) for authoring, and [EmitterView](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Time Rendering and Particles.md#emitterview>) for playback from code.

### Engine.Image

A static picture, stretched or tiled.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `drawAs`, `texture`, `textureRect`, `colour` |

**Draw As** is an enum selector in the Inspector: **Image** stretches the picture, while **Tile** repeats its texture region to fill the layout area and crops a partial final tile. Global UI Scale scales the complete result either way. An optional `textureRect` defines the tile. See [Engine: Layout and Image Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#image>).

### Engine.ProgressBar

A background layer with a fill revealed from left to right.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `size`, `progress`, `backgroundTexture`, `fillTexture`, `backgroundTextureRect`, `fillTextureRect`, `backgroundColor`, `fillColor` |

`progress` runs from 0 to 1; values outside that range are clamped and non-finite values become 0. Each texture defaults to empty for a solid-colour layer, and a non-empty value uses a canonical `/Game/Assets/...` path. The fill is cropped to the current progress rather than compressed into it. Colours tint their own layer, so `[255,255,255,255]` preserves image colours. See [Engine: Interactive and Declarative UI Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#progressbar>).

### Engine.Rect

A rectangle drawn from a window skin — panels, frames and selection highlights.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `size`, `windowSkin`, `opacityCurve` |

`opacityCurve` is relative to `Data/Curves`, with no directory prefix or extension, and resolves to a scalar curve. A Button with an empty `texture` uses the current window skin's selection-Rect image. See [Engine: Layout and Image Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#rect>).

### Engine.SolidRect

A rectangle filled with one colour, with an optional outline.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `size`, `fillColor`, `outlineColor`, `outlineThickness` |

`outlineThickness` defaults to 0 and `outlineColor` to fully transparent, so an unedited SolidRect is a plain fill. See [Engine: Layout and Image Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#solidrect>).

### Engine.Window

A skinned window frame — the standard background for dialogs, menus and message boxes.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Visual | none | — | `size`, `windowSkin`, `repeated`, `colour` |

`repeated` selects texture repetition for the background when true and stretching when false; borders always tile between the corners. `colour` is an RGBA tint defaulting to `[255,255,255,255]`, so set its alpha in JSON for a translucent frame. See [Engine: Text and Windows](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Text and Windows.md#window>).

## Input controls

Input controls accept focus and player input. Each one is a `FunctionalBase`, which accepts focus and input only while it and every `FunctionalBase` ancestor are active, and while it and every `ControlBase` ancestor are visible.

`FunctionalImage` is in this group rather than under Visual: it is an Image that also takes input.

### Engine.Button

A clickable button with hover and pressed tints and an optional gamepad binding.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `texture`, `textureRect`, `colour`, `hoverColour`, `pressedColour`, `gamepadButton`, `gamepadLongPress` |

An empty `texture` uses the current window skin's selection-Rect image. Default and custom backgrounds remain visible for every input device. The bound gamepad glyph is shown when `Engine.Input.getInputHintDevice()` selects Gamepad; size, global bounds, hit area and separate text controls stay unchanged. `gamepadButton` is a logical name such as `"X"` and defaults to empty; `gamepadLongPress` defaults to false. A Button accepts no children, so its label is a separate text control. See [Engine: Runtime Values and Functional Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#button>).

### Engine.CheckBox

A two-state toggle that draws its own label and reports changes.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, `checked`, `windowSkin`, *text style* |

The label uses the inline text-style group or a `textConfig`. `toggle()` flips the state from code, and `setOnCheckedChanged` receives the result. See [Engine: Interactive and Declarative UI Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#checkbox>).

### Engine.DropBox

A collapsed single-choice list that opens into a popup.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, `windowSkin`, *text style* |

Populate the options from the Controller with `setItems`; they are not saved as asset properties. See [Engine: Interactive and Declarative UI Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#dropbox>) for selection and sound callbacks.

### Engine.FunctionalImage

An Image that also accepts focus and input, and the base of CharacterView.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `drawAs`, `texture`, `textureRect`, `colour` |

It serialises the same properties as Image and inherits its `drawAs` API. Choose Image when nothing needs to click the picture. See [Engine: Runtime Values and Functional Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#functionalimage>).

### Engine.FunctionalPlainText

A PlainText that also accepts focus and input.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `text`, `colour`, *text style* |

See [Engine: Runtime Values and Functional Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#functionalplaintext>).

### Engine.FunctionalRichText

A RichText that also accepts focus and input.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `textConfig`, `text`, `colour` |

See [Engine: Runtime Values and Functional Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#functionalrichtext>).

### Engine.GamepadHintBar

Shows up to three gamepad hints, centred in its size.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, *text style* |

The hints appear only while a gamepad is connected. The Controller supplies up to three `Engine.GamepadHint` entries. See [Engine: Interactive and Declarative UI Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#gamepadhintbar>).

### Engine.Slider

A draggable numeric value within a range.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, `minValue`, `maxValue`, `value`, `lineTexture`, `handleTexture` |

`minValue` and `maxValue` are integers defaulting to 0 and 100, and the two textures default to `/Game/Assets/System/SliderLine.png` and `/Game/Assets/System/SliderHandle.png`. Editing covers pointer, keyboard and touch input, and reports through `setOnValueChanged`. See [Engine: Interactive and Declarative UI Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#slider>).

### Engine.TabView

A row of selectable tabs.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, `windowSkin`, `items`, *text style* |

`items` is a construction-only, non-empty `string[]`. Before the first visible frame the Controller supplies localised labels and binds selection behaviour. `selectedIndex` and key-hint tables are runtime-only, and `tabCount` is not accepted. See [Engine: Interactive and Declarative UI Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#tabview>).

### Engine.TextBox

An editable single-field text input.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Input | none | — | `size`, `windowSkin`, `text`, *text style* |

Set `text` for the initial runtime value; `previewText` affects the Designer only. See [Engine: Interactive and Declarative UI Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#textbox>) for editing and callbacks.

## Text controls

Text controls draw strings and take no input. Use the Functional variants when the text itself must be focusable or clickable.

### Engine.PlainText

Static text styled directly on the node.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Text | none | — | `text`, `colour`, *text style* |

See [Engine: Text and Windows](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Text and Windows.md#plaintext>).

### Engine.RichText

Static text with named tag styles resolved through a TextConfig.

| Category | Children | Slot | Key properties |
|---|---|---|---|
| Text | none | — | `textConfig`, `text`, `colour` |

See [Engine: Text and Windows](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Text and Windows.md#richtext>).

## Choosing between controls

| To do this | Use |
|---|---|
| Position children freely, or start a new screen | Canvas |
| Flow children into rows and columns | ListView |
| Scroll content that does not fit | ScrollBox |
| Repeat one template at equal spacing | WrapBox |
| Show a picture, stretched or tiled | Image, or FunctionalImage when it must take input |
| Show an animated character or enemy sheet | CharacterView |
| Show a particle effect | EmitterView |
| Show progress from 0 to 1 | ProgressBar |
| Draw a skinned dialog frame | Window |
| Draw a skinned panel or selection highlight | Rect |
| Draw a solid-colour block | SolidRect |
| Show static text | PlainText, or RichText for named tag styles |
| Show text that must be focusable | FunctionalPlainText or FunctionalRichText |
| Accept a click | Button |
| Accept a yes/no choice | CheckBox |
| Accept one choice from a list | DropBox |
| Accept a number within a range | Slider |
| Accept typed text | TextBox |
| Switch between pages | TabView |
| Show gamepad button hints | GamepadHintBar |

## Runtime-only types

`Engine.SectorShape` and `Engine.AssetInstance` are available to code but are not built-in Palette controls. See the [interactive control reference](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md>) for their APIs. Exposed project assets appear in the Palette by their own names, and their generated View exposes the runtime asset through `.instance`.

## Related pages

- [UI Asset Editor](<UI Asset Editor.md>)
- [UI Asset Schema and Control Registry](<../Lua and Blueprint Scripting/Declarative UI/Asset Schema and Control Registry.md>)
- [Declarative UI Authoring Workflow](<../Lua and Blueprint Scripting/Declarative UI/Authoring Workflow.md>)
- [Engine: Layout and Image Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md>)
- [Engine: Interactive and Declarative UI Controls](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md>)
- [Native UI Adapters](<../Native C++ Development/Native UI Adapters.md>)
