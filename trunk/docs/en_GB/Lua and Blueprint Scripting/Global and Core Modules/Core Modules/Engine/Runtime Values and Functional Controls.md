# Engine: Runtime Values and Functional Controls

This page documents dynamic runtime values (`RuntimeValue`), UI base classes (`ControlBase`, `FocusableMixin`, `FunctionalBase`), root composition canvases (`Canvas`), interactive buttons (`Button`) and functional control variants (`FunctionalImage`, `FunctionalPlainText`, `FunctionalRichText`). Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; static methods take no receiver.

## RuntimeValue

Registered under `Engine` for dynamic native and Lua values; Lua `any` values keep their ordinary representations. For C++ ownership and conversion rules, see [Core headers and Runtime boundary](<../../../../Native C++ Development/Build and Module Layout.md#core-headers-and-runtime-boundary>).

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `isNil` | — | `bool` | Pure. |
| `typeName` | — | `string` | Pure. |

## Button

**Inherits:** `Engine.Image`, `Engine.FunctionalBase`

Interactive image button supporting hover/pressed colour states and optional gamepad shortcut hints.

- **Background:** A declarative `Button` with an empty `texture` draws the current window skin's selection-rectangle image. Both the default background and custom textures stay visible across all input devices without changing size, global bounds, hit area or separate text controls.
- **Gamepad binding:** `gamepadButton` (logical name such as `"X"`, default `""` for unbound) and `gamepadLongPress` (default `false`) bind a gamepad trigger. In Lua, call `button:setGamepadButton(Engine.JoystickButton.getX())` (or `nil` to clear) and `button:setGamepadLongPress(true)`.
- **Gamepad hint visual:** While `Engine.Input.getInputHintDevice()` is `Engine.InputHintDevice.Gamepad`, the bound key renders as a `16`-logical-pixel circle vertically centred at the button's inner right edge (unaffected by background stretching), sharing the white circle, black glyph and green clockwise long-press fill with [`GamepadHintBar`](<Interactive and Declarative UI Controls.md#gamepadhintbar>). A disabled button's hint draws grey.
- **Gamepad triggering:** Short press fires `onConfirm` / `addConfirmCallback` once; long press fires once at one second and requires release before triggering again. The button does not need focus, but both it and its owning window must be visible and interactive (hiding, disabling, deactivating the window or disconnecting the gamepad clears progress).

### Properties

Declarative properties include `gamepadButton` (default `""`) and `gamepadLongPress` (default `false`) alongside the standard texture and colour settings.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setVisible` | `visible: bool` | — | — |
| `setColour` | `colour: sf.Color` | — | — |
| `getColour` | — | `sf.Color` | Pure. |
| `setHoverColour` | `colour: sf.Color` | — | — |
| `getHoverColour` | — | `sf.Color` | Pure. |
| `setPressedColour` | `colour: sf.Color` | — | — |
| `getPressedColour` | — | `sf.Color` | Pure. |
| `setGamepadButton` | `button: Engine.InputNamedValue \| nil` | — | — |
| `getGamepadButton` | — | `Engine.InputNamedValue \| nil` | Pure. |
| `setGamepadLongPress` | `longPress: bool` | — | — |
| `getGamepadLongPress` | — | `bool` | Pure. |

## Canvas

**Inherits:** `Engine.SpriteBase`, `Engine.FunctionalBase`

Offscreen UI composition surface owning a z-ordered child tree, animations, view transforms and an optional CPU particle system.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getOrigin` | — | `sf.Vector2f` | Pure. |
| `setOrigin` | `origin: sf.Vector2f` | — | — |
| `getSize` | — | `sf.Vector2u` | Pure. |
| `resize` | `size: sf.Vector2u` | — | — |
| `getNoTranslationRect` | — | `sf.IntRect` | Pure. |
| `getContentRect` | — | `sf.IntRect` | Pure. |
| `getView` | — | `sf.View` | Pure. |
| `getDefaultView` | — | `sf.View` | Pure. |
| `setView` | `view: sf.View` | — | — |
| `getChildren` | — | `Engine.ControlBase[]` | Pure. |
| `addChild` | `child: Engine.ControlBase` | — | — |
| `insertChildBefore` | `child: Engine.ControlBase`, `before: Engine.ControlBase` | — | — |
| `removeChild` | `child: Engine.ControlBase` | — | — |
| `addAnim` | `animation: Engine.AnimSprite` | — | — |
| `removeAnim` | `animation: Engine.AnimSprite` | — | — |
| `clearAnims` | — | — | — |
| `getAnims` | — | `Engine.AnimSprite[]` | Pure. |
| `getParticleSystem` | — | `Engine.ParticleSystem` | — |
| `setZOrder` | `zOrder: int` | — | — |
| `getZOrder` | — | `int` | Pure. |
| `update` | `deltaTime: float` | — | — |
| `render` | — | — | — |
| `lateUpdate` | `deltaTime: float` | — | — |
| `fixedUpdate` | `fixedDelta: float` | — | — |

`insertChildBefore(child, before)` inserts `child` immediately before an existing sibling in update order without detaching or resetting siblings (a `nil` child or missing anchor is rejected; visual stacking follows each child's z-order).

### CPU particles in a Canvas

- **Ownership and updates:** `getParticleSystem()` lazily creates and returns the Canvas's independent CPU `ParticleSystem`. `Canvas` drives its tick, late-tick and fixed-tick phases automatically (do not tick it again from Lua); hidden or unmounted Canvas trees pause under the normal UI lifecycle.
- **Rendering and coordinates:** Particles draw after ordinary children and animations and before overlays in Canvas logical coordinates and clipping. Create `TextParticle` with `logicalCoordinates = true`. Canvas composition applies display scale, transforms, colour and fade once (do not copy window fade onto particles); text configuration stays independent of particle transform and colour edits.
- **Cleanup:** Call `clear()` when closing or resetting an effect owner (the system stays reusable, and Canvas destruction clears it even if another reference keeps it alive). `ScrollBox` inherits this integration; `Engine.Window` is a skin visual rather than a Canvas host.

## ControlBase

Root base class for all UI controls, providing visibility, hierarchy, bounds, transforms and drawing.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getVisible` | — | `bool` | Pure. |
| `setVisible` | `visible: bool` | — | — |
| `getName` | — | `string` | Pure. |
| `setName` | `name: string` | — | — |
| `getParent` | — | `Engine.ControlBase` | Pure. |
| `setParent` | `parent: Engine.ControlBase` | — | — |
| `getChildren` | — | `Engine.ControlBase[]` | Pure. |
| `getSize` | — | `sf.Vector2f` | Pure. |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `getAbsoluteBounds` | — | `sf.FloatRect` | Pure. |
| `getRenderStates` | — | `sf.RenderStates` | Pure. |
| `setPosition` | `position: sf.Vector2f` | — | — |
| `getPosition` | — | `sf.Vector2f` | Pure. |
| `move` | `offset: sf.Vector2f` | — | — |
| `getRotation` | — | `sf.Angle` | Pure. |
| `setRotation` | `angle: sf.Angle` | — | — |
| `rotate` | `angle: sf.Angle` | — | — |
| `getScale` | — | `sf.Vector2f` | Pure. |
| `setScale` | `scale: sf.Vector2f` | — | — |
| `scale` | `factor: sf.Vector2f` | — | — |
| `getOrigin` | — | `sf.Vector2f` | Pure. |
| `setOrigin` | `origin: sf.Vector2f` | — | — |
| `getTransform` | — | `sf.Transform` | Pure. |
| `getInverseTransform` | — | `sf.Transform` | Pure. |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## FocusableMixin

Mixin providing keyboard/gamepad focus participation, focus-group binding and focus gain/loss hooks.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setCanReceiveFocus` | `canReceiveFocus: bool` | — | — |
| `getCanReceiveFocus` | — | `bool` | Pure. |
| `getFocused` | — | `bool` | Pure. |
| `setFocused` | `focused: bool` | — | — |
| `setFocusGroup` | `focusGroup: Engine.RuntimeObject` | — | — |
| `getFocusGroup` | — | `Engine.RuntimeObject` | Pure. |
| `onFocusGained` | — | — | — |
| `onFocusLost` | — | — | — |

## FunctionalBase

**Inherits:** `Engine.FocusableMixin`

Base interactive behaviour mixin providing pointer, touch, keyboard and gamepad input dispatch, capture and per-frame tick hooks.

### Input event data

Input methods and callbacks receive `Engine.UiInputEventArguments`. Pointer events populate `position: sf.Vector2f`; mouse-button events add `button: sf.Mouse.Button`; wheel events add `delta: number`. All fields are optional and omitted for keyboard events. Construct `Engine.UiInputEventArguments.new({})` for programmatic confirmation or cancellation.

### Interaction eligibility

A `FunctionalBase` accepts focus and input only while it and every `FunctionalBase` ancestor are active, and while it and every `ControlBase` ancestor are visible (`getActive` and `getVisible` query only local state). Losing eligibility clears pointer and touch capture across the subtree and keeps the cancelled gesture consumed until release, while ticking and rendering continue.

### Touch capture

- `setTouchHitBounds(bounds)` sets an optional touch-only hit rectangle in local logical coordinates (requires finite coordinates and non-negative size; `nil` restores default). `getAbsoluteTouchHitBounds()` returns its axis-aligned screen rectangle after display scale and hierarchy transforms (defaulting to the control's absolute bounds when mixed into a `ControlBase`, or `nil` for a standalone `FunctionalBase`).
- An eligible control captures any primary touch starting inside its absolute touch hit bounds and continues receiving motion and release even outside them. Releasing outside clears the pressed state without firing click or confirm while still consuming that tap so controls underneath do not activate. Multi-touch cancellation also releases capture. Touch hit bounds never affect mouse hit testing.

### Pointer click

A mouse press initiates pointer capture without firing `onClick`. `onClick` fires only when the pressed button releases while hovering the same control (releasing outside clears pressed state without clicking) and passes the released `button` so left-click handlers can filter on it. Touch taps follow touch capture above and prefer `onConfirm` when a confirm callback exists.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setKeyboardFocusResolver` | `resolver: function` | — | Static. |
| `setDirectionalFocusRequester` | `requester: function` | — | Static. |
| `setKeyboardFocusSetter` | `setter: function` | — | Static. |
| `setKeyboardCursorResolver` | `resolver: function` | — | Static. |
| `canReceiveFocus` | — | `bool` | Pure. |
| `shouldDispatchKeyboardInput` | — | `bool` | Pure. |
| `requestDirectionalFocusMove` | `direction: string` | `bool` | — |
| `requestKeyboardFocus` | — | `bool` | — |
| `ownsKeyboardCursorFocus` | — | `bool` | Pure. |
| `isHovered` | — | `bool` | Pure. |
| `isPressed` | — | `bool` | Pure. |
| `getActive` | — | `bool` | Pure. |
| `setActive` | `active: bool` | — | — |
| `setTouchHitBounds` | `bounds: sf.FloatRect \| nil` | — | — |
| `getAbsoluteTouchHitBounds` | — | `sf.FloatRect \| nil` | Pure. |
| `addConfirmCallback` | `callback: function` | — | — |
| `addCancelCallback` | `callback: function` | — | — |
| `addClickCallback` | `callback: function` | — | — |
| `addMouseButtonDownCallback` | `callback: function` | — | — |
| `addHoverCallback` | `callback: function` | — | — |
| `addUnHoverCallback` | `callback: function` | — | — |
| `addMouseMovedCallback` | `callback: function` | — | — |
| `addMouseWheelScrolledCallback` | `callback: function` | — | — |
| `addKeyDownCallback` | `callback: function` | — | — |
| `addKeyUpCallback` | `callback: function` | — | — |
| `update` | `deltaTime: float` | — | — |
| `lateUpdate` | `deltaTime: float` | — | — |
| `fixedUpdate` | `fixedDelta: float` | — | — |
| `onConfirm` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onCancel` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onClick` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseButtonDown` | `arguments: Engine.UiInputEventArguments` | `bool` | — |
| `onHover` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onUnHover` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseMoved` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseWheelScrolled` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onKeyDown` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onKeyUp` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onTick` | `deltaTime: float` | — | — |
| `onLateTick` | `deltaTime: float` | — | — |
| `onFixedTick` | `fixedDelta: float` | — | — |

## FunctionalImage

**Inherits:** `Engine.Image`, `Engine.FunctionalBase`

Interactive `Image` control combining `Image` rendering with `FunctionalBase` input and focus handling.

## FunctionalPlainText

**Inherits:** `Engine.PlainText`, `Engine.FunctionalBase`

Interactive `PlainText` control combining `PlainText` rendering with `FunctionalBase` input and focus handling.

## FunctionalRichText

**Inherits:** `Engine.RichText`, `Engine.FunctionalBase`

Interactive `RichText` control combining `RichText` rendering with `FunctionalBase` input and focus handling.
