# Engine: Text and Windows

This page documents text configuration builders and measurement helpers (`Engine.TextConfig`, `Engine.TextLayout`), text styling records (`TextOutlineConfig`, `TextGlowConfig`, `TextGradientConfig`, `TextStyle`, `PlainTextConfig`, `RichTextConfig`), text controls (`PlainText`, `RichText`) and nine-slice windows (`Window`). Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; free functions take no receiver.

## Engine.TextConfig

Builds validated plain and rich text configurations for `Global.Data` and Declarative UI. Input tables must be JSON-compatible; functions, native objects and cycles are rejected. For editor authoring, see [Text Config](<../../../../Editor User Guide/General Data and Text Config.md#text-config>).

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `buildPlain` | `data: table`, `sourceName: string` | `Engine.PlainTextConfig` | — |
| `buildRich` | `data: table`, `sourceName: string` | `Engine.RichTextConfig` | — |

## Engine.TextLayout

Measures, truncates and wraps UTF-8 text without mutating the supplied text control; `maxWidth` must be positive and finite.

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `measurePlainText` | `control: Engine.PlainText`, `text: string` | `number` | — |
| `measureRichText` | `source: Engine.RichText`, `text: string` | `number` | — |
| `fitPlainText` | `text: string`, `maxWidth: number`, `control: Engine.PlainText` | `string` | Returns `text` unchanged if it fits; otherwise truncates at grapheme boundaries and appends `.` within `maxWidth` (or returns `""` if even `.` does not fit). |
| `wrapPlainText` | `text: string`, `maxWidth: number`, `control: Engine.PlainText` | `string` | — |
| `wrapRichText` | `text: string`, `maxWidth: number`, `source: Engine.RichText` | `string` | — |

Rich-text measurement and wrapping apply the supplied control's authored configuration (including named tag styles) without altering its text or playback state.

## TextOutlineConfig

Configures outline stroke colour and thickness for plain text.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `color` | `sf.Color` | — | — |
| `thickness` | `float` | `0` | — |

## TextGlowConfig

Configures soft glow rendering around plain or rich text.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `enabled` | `bool` | `false` | — |
| `color` | `sf.Color` | — | — |
| `radius` | `float` | `0` | — |
| `intensity` | `float` | `0` | — |

## TextGradientConfig

Configures colour-curve gradient shading across text glyphs. Use the read-only constants `Engine.TextGradientConfig.HORIZONTAL` (`"horizontal"`) and `VERTICAL` (`"vertical"`) for `direction` (default `VERTICAL`).

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `enabled` | `bool` | `false` | — |
| `direction` | `string` | `"vertical"` | `"horizontal"` or `"vertical"`. |
| `curve` | `Engine.Vector4Curve` | — | — |

## TextStyle

Defines character typography, spacing, fill and outline attributes for a rich-text style span.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `characterSize` | `int` | — | — |
| `bold` | `bool` | — | — |
| `italic` | `bool` | — | — |
| `underlined` | `bool` | — | — |
| `strikeThrough` | `bool` | — | — |
| `fillColor` | `sf.Color` | — | — |
| `letterSpacing` | `float` | — | — |
| `lineSpacing` | `float` | — | — |
| `outlineColor` | `sf.Color` | — | — |
| `outlineThickness` | `float` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `enableStyle` | `text: sf.Text` | — | — |
| `adaptStyle` | `inStyle: Engine.TextStyle` | — | — |

## PlainTextConfig

Validated configuration record for `Engine.PlainText` controls and `Engine.TextParticle` instances.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `type` | `string` | `"plainTextConfig"` | — |
| `name` | `string` | — | — |
| `font` | `sf.Font` | — | — |
| `characterSize` | `int` | `30` | — |
| `style` | `uint32_t` | — | — |
| `slantAngle` | `float` | `0` | — |
| `fillColor` | `sf.Color` | — | — |
| `letterSpacing` | `float` | `1` | — |
| `lineSpacing` | `float` | `1` | — |
| `lineAlignment` | `sf.Text.LineAlignment` | — | — |
| `outline` | `Engine.TextOutlineConfig` | — | — |
| `glow` | `Engine.TextGlowConfig` | — | — |
| `gradient` | `Engine.TextGradientConfig` | — | — |

## RichTextConfig

Validated configuration record for `Engine.RichText` controls, including named tag styles.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `type` | `string` | `"richTextConfig"` | — |
| `name` | `string` | — | — |
| `font` | `sf.Font` | — | — |
| `lineAlignment` | `sf.Text.LineAlignment` | — | — |
| `defaultStyle` | `Engine.TextStyle` | — | — |
| `styleOrder` | `string[]` | — | — |
| `styles` | `any` | — | — |
| `glow` | `Engine.TextGlowConfig` | — | — |
| `gradient` | `Engine.TextGradientConfig` | — | — |

## PlainText

**Inherits:** `Engine.ControlBase`

Single-style UTF-8 text control backed by `Engine.PlainTextConfig`.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getConfig` | — | `Engine.PlainTextConfig` | Pure. |
| `getCharacterSize` | — | `int` | Pure. |
| `setString` | `text: string` | — | — |
| `getString` | — | `string` | Pure. |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `getGlobalBounds` | — | `sf.FloatRect` | Pure. |
| `getSize` | — | `sf.Vector2f` | Pure. |
| `getOrigin` | — | `sf.Vector2f` | Pure. |
| `setOrigin` | `origin: sf.Vector2f` | — | — |
| `getColour` | — | `sf.Color` | Pure. |
| `setColour` | `colour: sf.Color` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## RichText

**Inherits:** `Engine.ControlBase`

Tagged multi-style text control backed by `Engine.RichTextConfig`.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getConfig` | — | `Engine.RichTextConfig` | Pure. |
| `setString` | `text: string` | — | — |
| `getString` | — | `string` | Pure. |
| `setColour` | `colour: sf.Color` | — | — |
| `getColour` | — | `sf.Color` | Pure. |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `getGlobalBounds` | — | `sf.FloatRect` | Pure. |
| `getSize` | — | `sf.Vector2f` | Pure. |
| `getOrigin` | — | `sf.Vector2f` | Pure. |
| `setOrigin` | `origin: sf.Vector2f` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## Window

**Inherits:** `Engine.SpriteBase`

Nine-slice window-skin visual control. `repeated` tiles the background when `true` and stretches it when `false`; borders always tile between corners. `DropBox` popups use the same window-skin rules.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getSize` | — | `sf.Vector2u` | Pure. |
| `resize` | `size: sf.Vector2u` | — | — |
| `setWindowSkin` | `windowSkin: sf.Image`, `repeated: bool` | — | — |
