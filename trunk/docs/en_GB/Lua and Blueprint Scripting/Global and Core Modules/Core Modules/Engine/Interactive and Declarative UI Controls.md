# Engine: Interactive and Declarative UI Controls

This page documents interactive UI controls (`TextBox`, `CheckBox`, `DropBox`, `TabView`, `GamepadHintBar`, `ProgressBar`, `Slider`), the pie-slice graphic primitive (`SectorShape`) and instantiated declarative UI handles (`AssetInstance`). Instance method tables omit the implicit `self` receiver, which Blueprint nodes expose as the first target pin (defaulting to the graph owner when unset); static constructors (`new`) take no receiver.

## TextBox

**Inherits:** `Engine.ControlBase`, `Engine.FunctionalBase`

Single-line UTF-8 text input control with editing initially off. Click, tap or Confirm enters editing mode; focus alone does not.

- **Desktop editing:** Supports a blinking caret, selection, horizontal scrolling, grapheme-based navigation and deletion, Home/End and clipboard shortcuts. On Windows, both embedded play and standalone windows support character input and IME composition; losing the bridge connection ends editing under the same committed-text rules as focus loss.
- **Confirming and cancelling:** Enter or `finishEdit` commits the edited text, whereas Escape, a connected controller's B/Cancel button or `cancelEdit` restores the value captured when editing started (neither submits an enclosing form). Focus loss keeps committed text and discards unfinished composition. Hiding, disabling, detaching or destroying the control ends editing. Only one `TextBox` edits at a time.
- **Mobile and HarmonyOS:** iOS, Android and HarmonyOS phones and tablets open a system input dialog (HarmonyOS 2-in-1 uses desktop editing). Native Done commits the text and Cancel restores the pre-dialog value; backgrounding closes the session without reopening it on return.
- **Callbacks and sanitisation:** `setOnTextChanged` fires on committed-value changes (including `setString` and cancellation), while IME composition does not change `getString()`. Newlines and tabs convert to spaces and other control characters are stripped. Form validation belongs to the Controller.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `new` | `size: sf.Vector2f`, `windowSkin: sf.Image`, `textConfig: Engine.PlainTextConfig`, `text: string = ""` | `Engine.TextBox` | Static. |
| `getString` | — | `string` | — |
| `setString` | `text: string` | — | — |
| `isEditing` | — | `bool` | — |
| `beginEdit` | — | `bool` | — |
| `finishEdit` | — | — | — |
| `cancelEdit` | — | — | — |
| `setInputDialogLabels` | `title: string`, `done: string`, `cancel: string` | — | — |
| `setOnTextChanged` | `callback: function(text: string) \| nil` | — | — |
| `setOnEditingChanged` | `callback: function(editing: bool) \| nil` | — | — |
| `getSize` | — | `sf.Vector2f` | — |
| `resize` | `size: sf.Vector2f` | — | — |
| `setWindowSkin` | `windowSkin: sf.Image` | — | — |
| `setTextConfig` | `textConfig: Engine.PlainTextConfig` | — | — |

## CheckBox

**Inherits:** `Engine.ControlBase`, `Engine.FunctionalBase`

Binary toggle control rendered with a window skin and plain-text configuration.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | Pure. |
| `resize` | `size: sf.Vector2f` | — | — |
| `isChecked` | — | `bool` | Pure. |
| `setChecked` | `checked: bool` | — | — |
| `toggle` | — | — | — |
| `setWindowSkin` | `windowSkin: sf.Image` | — | — |
| `setTextConfig` | `textConfig: Engine.PlainTextConfig` | — | — |
| `setOnCheckedChanged` | `callback: function` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `update` | `deltaTime: float` | — | — |
| `onConfirm` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onClick` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseButtonDown` | `arguments: Engine.UiInputEventArguments` | `bool` | — |
| `onKeyDown` | `arguments: Engine.UiInputEventArguments` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## DropBox

**Inherits:** `Engine.ControlBase`, `Engine.FunctionalBase`

Drop-down selection control displaying a collapsed button and a scrollable overlay popup list.

### Overlay and measurement

- **Overlay rendering:** Under a `Canvas` ancestor, the expanded popup draws in the outermost ancestor `Canvas` overlay layer after ordinary content and animations, escaping nested `Canvas` clipping without resizing or moving sibling controls.
- **Direction and height:** Visible popup height clamps to the intersection of the root `Canvas` content rectangle and the game screen. The popup opens downwards if the full list fits, upwards if it fits there instead, or towards the side with more space with internal scrolling.
- **Bounds:** `getSize()` always returns the collapsed layout size. `getLocalBounds()` returns the collapsed rectangle while closed, and its union with the visible popup while expanded (an upward popup can produce a negative local Y). Inherited absolute bounds and default touch hit bounds follow this extent.

### Popup scrolling and selection

An ordinary mouse wheel scrolls towards a row-sized target, while a precise wheel and mobile/Web touch dragging apply offsets directly. Scrolling moves only the popup viewport — never the cursor or committed selection — and releasing a drag never confirms an item.

Directional keys move the cursor and scroll it into view. Mouse hover previews a visible item and clicking confirms it; mobile and Web taps use two-stage confirmation. All indices address the complete `items` array.

### Touch confirmation

On mobile and Web, an expanded `DropBox` uses two-stage confirmation: tapping a non-highlighted item moves the highlight and plays the cursor sound without changing `selectedIndex` or firing selection callbacks; tapping the highlighted item (including the initial highlight) confirms it. Cancelling discards the preview and keeps `selectedIndex`.

### Selection callbacks

- `setOnSelectedIndexChanged(callback)` fires with the new index only when `setSelectedIndex` resolves to a different selection, running before the confirmation callback during confirmation.
- `setOnSelectionConfirmed(callback)` fires with the current index whenever an expanded, non-empty `DropBox` confirms (even if the index is unchanged). Programmatic selection, item replacement, cancellation and empty confirmation do not trigger it.
- Pass `nil` to either setter to clear its callback.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | Pure. |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `resize` | `size: sf.Vector2f` | — | — |
| `setWindowSkin` | `windowSkin: sf.Image`, `repeated: bool = false` | — | — |
| `setTextConfig` | `textConfig: Engine.PlainTextConfig` | — | — |
| `getItems` | — | `string[]` | Pure. |
| `setItems` | `items: string[]` | — | — |
| `getSelectedIndex` | — | `int` | Pure. |
| `setSelectedIndex` | `index: int` | — | — |
| `getSelectedItem` | — | `string` | Pure. |
| `isExpanded` | — | `bool` | Pure. |
| `setExpanded` | `expanded: bool` | — | — |
| `open` | — | — | — |
| `cancel` | — | — | — |
| `setOnSelectedIndexChanged` | `callback: function` | — | — |
| `setOnSelectionConfirmed` | `callback: function` | — | — |
| `setOnExpandedChanged` | `callback: function` | — | — |
| `setOpenSound` | `filename: string` | — | — |
| `getOpenSound` | — | `string` | Pure. |
| `setCursorSound` | `filename: string` | — | — |
| `getCursorSound` | — | `string` | Pure. |
| `setSelectSound` | `filename: string` | — | — |
| `getSelectSound` | — | `string` | Pure. |
| `setCancelSound` | `filename: string` | — | — |
| `getCancelSound` | — | `string` | Pure. |
| `update` | `deltaTime: float` | — | — |
| `onConfirm` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onCancel` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onClick` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseButtonDown` | `arguments: Engine.UiInputEventArguments` | `bool` | — |
| `onMouseMoved` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseWheelScrolled` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onKeyDown` | `arguments: Engine.UiInputEventArguments` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## TabView

**Inherits:** `Engine.ControlBase`, `Engine.FunctionalBase`

Horizontal tab strip with a fixed item count, optional keyboard/gamepad edge hints and non-focusable navigation.

### Selection and navigation

- **Layout and construction:** A `TabView` has a fixed, non-zero item count, zero-based clamped indices and an always-visible selection. Equal tab slots occupy the centre, with `16` logical pixels reserved on each side for hints. It never takes keyboard focus. Declarative assets establish the fixed count from the required non-empty `items: string[]` property (`tabCount` is not an asset property).
- **Updating items and selection:** `setItems(items)` replaces labels when the count matches without changing selection or callbacks (a mismatched count leaves the control unchanged). `setOnSelectedIndexChanged(callback)` sets or clears (`nil`) the callback. A real `setSelectedIndex` change invokes the callback silently; `selectPrevious`, `selectNext` and real Q/E, LB/RB, mouse-move or touch-tap changes play the cursor sound once and then invoke the callback. Unchanged and boundary selections do neither, and tabs do not wrap.
- **Input handling:** Focused content calls `handleNavigationInput()`, which consumes Q, E, LB and RB while the `TabView` is visible and active (duplicate same-direction inputs move once; opposing inputs cancel). Mouse movement selects central slots; left click there is consumed without selecting, while right/middle clicks and hint gutters pass through. Touch tap selects; touch movement does not.

### Key hints

`setKeyHint(leftHint, rightHint)` accepts two `Engine.KeyHint` values with optional `Keyboard` (non-Unknown `sf.Keyboard.Key`) and `Joystick` (`Engine.JoystickButton` getter value) fields. An empty hint clears that side, a missing current-device field hides it, and invalid input leaves both hints unchanged.

```lua
tabView:setKeyHint(
    Engine.KeyHint.new({ Keyboard = sf.Keyboard.Key.Q, Joystick = Engine.JoystickButton.getLB() }),
    Engine.KeyHint.new({ Keyboard = sf.Keyboard.Key.E, Joystick = Engine.JoystickButton.getRB() })
)
```

Hints appear only where movement is possible: a connected joystick shows `Joystick`, otherwise desktop, HarmonyOS 2-in-1 and HarmonyOS tablets show `Keyboard`, while Android, iOS and HarmonyOS phones hide hints. Hints never rebind or disable input.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | Pure. |
| `resize` | `size: sf.Vector2f` | — | — |
| `setWindowSkin` | `windowSkin: sf.Image` | — | — |
| `setTextConfig` | `textConfig: Engine.PlainTextConfig` | — | — |
| `getItems` | — | `string[]` | Pure. |
| `setItems` | `items: string[]` | — | — |
| `setOnSelectedIndexChanged` | `callback: function \| nil` | — | — |
| `getSelectedIndex` | — | `int` | Pure. |
| `setSelectedIndex` | `index: int` | — | — |
| `getSelectedItem` | — | `string` | Pure. |
| `selectPrevious` | — | `bool` | — |
| `selectNext` | — | `bool` | — |
| `handleNavigationInput` | — | `bool` | — |
| `setCursorSound` | `filename: string` | — | — |
| `getCursorSound` | — | `string` | Pure. |
| `setKeyHint` | `leftHint: Engine.KeyHint`, `rightHint: Engine.KeyHint` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `update` | `deltaTime: float` | — | — |
| `onClick` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseButtonDown` | `arguments: Engine.UiInputEventArguments` | `bool` | — |
| `onMouseMoved` | `arguments: Engine.UiInputEventArguments` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## GamepadHintBar

**Inherits:** `Engine.ControlBase`, `Engine.FunctionalBase`

Displays up to three centred gamepad button prompts while a gamepad is connected, rejecting more than three in `setHints`. Each entry is an `Engine.GamepadHint` with `Button` (`Engine.JoystickButton` getter value), `LongPress` and `Text`. Default windows place it along the bottom border via the shared `WindowChrome` asset, with `WindowBase` binding its callbacks.

A short-press hint draws the active controller's glyph on a `16`-logical-pixel white circle followed by its label. A long-press hint adds a green clockwise sector filling over one second, firing `setOnHintTriggered` once with the zero-based index at one second (releasing resets progress). `setHintEnabled` greys out a hint, stops its progress and blocks triggering.

### Properties

No editable properties are declared beyond `size` and the plain-text configuration.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | Pure. |
| `resize` | `size: sf.Vector2f` | — | — |
| `setTextConfig` | `textConfig: Engine.PlainTextConfig` | — | — |
| `setHints` | `hints: Engine.GamepadHint[]` | — | — |
| `getHintCount` | — | `int` | Pure. |
| `setHintEnabled` | `index: int`, `enabled: bool` | — | — |
| `isHintEnabled` | `index: int` | `bool` | Pure. |
| `isGamepadConnected` | — | `bool` | Pure. |
| `setOnHintTriggered` | `callback: function \| nil` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `update` | `deltaTime: float` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## SectorShape

**Inherits:** `sf.Drawable`, `sf.Transformable`

Base graphic primitive (not a UI control and not in the Palette) that fills a pie slice clockwise from 12 o'clock with its angle clamped to `0–360` degrees. Used by `GamepadHintBar` and `Button` for long-press progress indicators.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `new` | `radius: float = 0.0`, `fillColour: sf.Color = white` | `Engine.SectorShape` | Static. |
| `getRadius` | — | `float` | Pure. |
| `setRadius` | `radius: float` | — | — |
| `getAngle` | — | `sf.Angle` | Pure. |
| `setAngle` | `angle: sf.Angle` | — | — |
| `getFillColour` | — | `sf.Color` | Pure. |
| `setFillColour` | `colour: sf.Color` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `getGlobalBounds` | — | `sf.FloatRect` | Pure. |

## ProgressBar

**Inherits:** `Engine.ControlBase`

Horizontal bar control with separate background and fill layers supporting solid colours or cropped textures.

- **Progress range:** Runs from `0` to `1`; out-of-range values clamp and non-finite values become `0`. The background spans the full control, while the fill reveals the left portion proportionally by cropping rather than compressing its image. Display scale and presentation opacity apply to both layers.
- **Textures and tinting:** Each layer independently retains its `sf.Texture`; passing `nil` clears the texture and restores solid-colour rendering. Texture rectangles use source pixels (`nil` selects the full image). Each layer's colour multiplies its texture (use white to preserve original colours). Replacing or clearing textures preserves size and progress.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | Pure. |
| `resize` | `size: sf.Vector2f` | — | — |
| `getProgress` | — | `float` | Pure. |
| `setProgress` | `progress: float` | — | — |
| `getBackgroundColor` | — | `sf.Color` | Pure. |
| `setBackgroundColor` | `color: sf.Color` | — | — |
| `getFillColor` | — | `sf.Color` | Pure. |
| `setFillColor` | `color: sf.Color` | — | — |
| `getBackgroundTexture` | — | `sf.Texture \| nil` | Pure. |
| `setBackgroundTexture` | `texture: sf.Texture \| nil` | — | — |
| `getFillTexture` | — | `sf.Texture \| nil` | Pure. |
| `setFillTexture` | `texture: sf.Texture \| nil` | — | — |
| `getBackgroundTextureRect` | — | `sf.IntRect \| nil` | Pure. |
| `setBackgroundTextureRect` | `rect: sf.IntRect \| nil` | — | — |
| `getFillTextureRect` | — | `sf.IntRect \| nil` | Pure. |
| `setFillTextureRect` | `rect: sf.IntRect \| nil` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## Slider

**Inherits:** `Engine.ControlBase`, `Engine.FunctionalBase`

Integer range slider with customisable track and handle textures.

### Touch interaction

On touch capture, a `Slider` updates its value from the pointer's X coordinate relative to its visual bounds and continues updating and clamping until release, even outside hit bounds. `setTouchHitBounds` alters only the touch target, not the visual or mouse bounds.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setVisible` | `visible: bool` | — | — |
| `getSize` | — | `sf.Vector2f` | Pure. |
| `resize` | `size: sf.Vector2f` | — | — |
| `getValue` | — | `int` | Pure. |
| `setValue` | `value: int` | — | — |
| `setRange` | `minValue: int`, `maxValue: int` | — | — |
| `getRange` | — | `minimum: int`, `maximum: int` | Pure. |
| `setValueFromRatio` | `ratio: float` | — | — |
| `setValueFromBoundsPosition` | `bounds: sf.FloatRect`, `position: sf.Vector2f` | — | — |
| `adjust` | `delta: int` | — | — |
| `getHandlePosition` | — | `int` | Pure. |
| `setLineTexture` | `texture: sf.Texture` | — | — |
| `setHandleTexture` | `texture: sf.Texture` | — | — |
| `setOnValueChanged` | `callback: function` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `update` | `deltaTime: float` | — | — |
| `onClick` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseButtonDown` | `arguments: Engine.UiInputEventArguments` | `bool` | — |
| `onMouseMoved` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onKeyDown` | `arguments: Engine.UiInputEventArguments` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## AssetInstance

Native handle exposed as `.instance` on generated declarative UI Views. Native `requireAsset` returns an `Engine.AssetInstance`, whereas `UiController:requireAsset` returns a generated child View; see [Declarative UI Runtime Contract](<../../../Declarative UI/Runtime Contract.md>).

- **Typed properties:** `setProperty` requires each property's declared runtime type — for example, `Engine.ImageDrawAs.Image` or `Engine.ImageDrawAs.Tile` for `drawAs` (JSON strings are rejected). `WrapBox` templates form separate instance boundaries; access repeated roots through `wrapBox:get(i)` instead of looking up template names in the enclosing asset.
- **Scoped reflow:** `reflowControl(localName, logicalSize)` applies a positive `sf.Vector2u` size to a named local system control and lays out its authored descendants while keeping its position. It does not cross nested asset boundaries (call `reflowControl` on the child instance instead), and the override persists until a parent reflow reapplies the authored Slot.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getRoot` | — | `Engine.ControlBase` | Pure. |
| `requireControl` | `localName: string` | `Engine.ControlBase` | Pure. |
| `getNodeByName` | `name: string` | `Engine.ControlBase` | Pure. |
| `requireAsset` | `localName: string` | `Engine.AssetInstance` | Pure. |
| `setProperty` | `localName: string`, `propertyId: string`, `value: Union[nil, bool, int, float, string, sf.Vector2f, sf.Vector2u, sf.IntRect, sf.Color, Engine.ImageDrawAs, string[]]` | — | — |
| `setText` | `localName: string`, `text: string` | — | — |
| `reflow` | `logicalSize: sf.Vector2u = nil` | — | — |
| `reflowControl` | `localName: string`, `logicalSize: sf.Vector2u` | — | — |
| `hasAnimation` | `name: string`, `target: string = nil` | `bool` | Pure. |
| `playAnimation` | `name: string`, `target: string = nil`, `onFinished: function = nil` | `bool` | — |
| `stopAnimation` | `name: string`, `target: string = nil` | — | — |
