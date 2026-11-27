# Engine: Input and Services

This page documents logical gamepad buttons (`JoystickButton`), WebGL input rules, input records (`InputActionKey`, `InjectedInputEvent`, `JoystickAxisEvent`), input hint tracking, touch/WebView/text-session capture, and the `Input` singleton and `Service` class. `Engine.Service` instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; `Engine.Input` exposes matching static singleton wrappers with no receiver.

## JoystickButton

`Engine.JoystickButton` returns logical button tokens (`Engine.InputNamedValue`) via `getA()`, `getB()`, `getX()`, `getY()`, `getLB()`, `getRB()`, `getView()`, `getMenu()`, `getLS()`, `getRS()`, `getXBox()` and `getShare()`.

- **Device-specific resolution:** Pass the complete `Engine.InputNamedValue` to Input queries and UI bindings. Ludork resolves it per connected controller when reading input, so cached bindings survive hot-plugging and mixed Xbox/PlayStation connections. Its `value` field stores the platform's default token number rather than a connected PlayStation controller's raw button index.
- **Raw vs logical queries:** Integer Input queries and `sf.Joystick` keep raw SFML button numbers. `Input.isJoystickButtonDown(id, button)` and `Input.isAnyJoystickButtonDown(button)` accept either a logical getter value or a raw integer and respect input blocking and focus. Getter availability does not guarantee a physical button on every controller; `getShare()` is unavailable on Android, HarmonyOS and Web.

Raw backend button indices by platform and controller family:

| Platform / device | A / B / X / Y | LB / RB | View / Menu | LS / RS | XBox / Share |
|---|---|---|---|---|---|
| Windows Xbox | `0/1/2/3` | `4/5` | `6/7` | `8/9` | `10/11` |
| macOS Xbox | `0/1/3/4` | `6/7` | `10/11` | `13/14` | `12/15` |
| Windows / macOS PlayStation | `1/2/0/3` | `4/5` | `8/9` | `10/11` | `12` / unavailable |
| Android, HarmonyOS | `0/1/3/4` | `6/7` | `13/12` | `10/11` | `14` / unavailable |
| iOS | `0/1/3/4` | `6/7` | `13/12` | `10/11` | `14/15` (Xbox); Share unavailable (PlayStation) |
| Web standard gamepad | `0/1/2/3` | `4/5` | `8/9` | `10/11` | `16` / unavailable |

Controller mapping and UI hint details:

- **PlayStation support:** First-party support covers DUALSHOCK 4, DualSense and DualSense Edge. Unidentified devices use the platform default layout (no third-party profiles). Each SFML joystick slot tracks its own identity and held/repeat state. On PlayStation: A/B/X/Y map to Cross/Circle/Square/Triangle, LB/RB to L1/R1, LS/RS to L3/R3, Menu to Options, XBox to PS, and View to Share (DUALSHOCK 4) or Create (DualSense/Edge); Xbox Share has no equivalent.
- **Mobile normalisation:** Android, HarmonyOS and iOS normalise supported controllers natively rather than using the desktop PlayStation raw table. Left-stick `X/Y` and D-pad `PovX/PovY` follow the direction rules below; other axes stay raw SFML axes.
- **UI hints and long press:** `Button`, `GamepadHintBar` and `TabView` hints track the most recently active controller (or lowest connected slot before activity). Switching or disconnecting a controller clears unfinished long-press progress; a one-second hold must come from a single controller.
- **iOS `GCExtendedGamepad`:** Uses Apple's [GCExtendedGamepad](https://developer.apple.com/documentation/gamecontroller/gcextendedgamepad) profile with left/right trigger presses at `8/9` and unused slots `2/5`. Optional stick-click, View, Home/XBox and [GCXboxGamepad](https://developer.apple.com/documentation/gamecontroller/gcxboxgamepad) Share inputs depend on device and OS version; system-reserved Home/Share gestures may not reach the game.
- **iOS axis signs:** Both stick Y axes are inverted from Apple's profile while D-pad Y keeps Apple's sign. The SFML backend does not assign Confirm, Cancel or movement actions.

| iOS / Web input | SFML axes | Values and direction |
|---|---|---|
| Left stick | `X/Y` | `-100` to `100`; right is positive `X`, down is positive `Y` |
| Right stick | `Z/R` | `-100` to `100`; right is positive `Z`, down is positive `R` |
| Left / right triggers | `U/V` | `0` to `100`; released is `0`, fully pressed is `100` |
| D-pad | `PovX/PovY` | `-100` to `100`; right is positive `PovX`, up is positive `PovY` |

## Web input

A single WebGL package handles keyboard, mouse and touch on desktop and mobile browsers, reporting `PLATFORM == "web"`:

- **Touch:** Uses standard capture, tap, drag, long-press and two-finger cancel rules mapped to the canvas drawing buffer and current scale. Browser touch cancellation, focus loss and page hiding release captures without confirming items or leaving held buttons, and touch does not emit duplicate compatibility mouse clicks. Input hints follow the most recently used device.
- **Text input:** Text boxes open a modal browser input field with their configured title, prompt, placeholder and confirm/cancel labels (tap the field if the browser does not auto-focus the software keyboard). Composition and selection work for Chinese and emoji; Enter does not submit unfinished composition, Cancel or Escape restores the original text, hiding the page cancels the session, and keyboard input stays captured while the dialog is open.
- **Gamepads:** Uses the browser's `standard` mapping and the tables above, clearing held and repeat states on disconnect. Unmapped controllers or browsers without Gamepad support report no device without blocking startup; browser- or system-reserved controls may not reach the game.

## InputActionKey

Binds a keyboard, mouse or gamepad input to an action. A `JoystickButton` action with a logical `name` resolves its getter token `code` per device; use an empty `name` with a numeric `code` for a raw button binding. Default Confirm and Cancel actions use logical A and B bindings.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `kind` | `InputActionKind` | — | — |
| `name` | `string` | — | — |
| `code` | `int` | `0` | — |
| `threshold` | `float` | `0` | — |

## InjectedInputEvent

Describes a synthetic input event passed to `Service.injectEvent`. Use the read-only class constants on `Engine.InjectedInputEvent` for `type` and `button`; the underlying properties and protocol retain their string values.

| `Engine.InjectedInputEvent` constants | String values |
|---|---|
| `KEY_PRESSED`, `KEY_RELEASED` | `"KeyPressed"`, `"KeyReleased"` |
| `MOUSE_MOVED` | `"MouseMoved"` |
| `MOUSE_BUTTON_PRESSED`, `MOUSE_BUTTON_RELEASED` | `"MouseButtonPressed"`, `"MouseButtonReleased"` |
| `MOUSE_WHEEL_SCROLLED` | `"MouseWheelScrolled"` |
| `MOUSE_ENTERED`, `MOUSE_LEFT` | `"MouseEntered"`, `"MouseLeft"` |
| `TEXT_ENTERED`, `TEXT_PREEDIT`, `TEXT_COMPOSITION` | `"TextEntered"`, `"TextPreedit"`, `"TextComposition"` |
| `FOCUS_GAINED`, `FOCUS_LOST` | `"FocusGained"`, `"FocusLost"` |
| `BUTTON_LEFT`, `BUTTON_RIGHT`, `BUTTON_MIDDLE` | `"Left"`, `"Right"`, `"Middle"` |
| `BUTTON_EXTRA1`, `BUTTON_EXTRA2` | `"Extra1"`, `"Extra2"` |

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `type` | `string` | — | — |
| `key` | `string` | — | — |
| `scan` | `int` | — | — |
| `button` | `string` | `"Left"` | — |
| `x` | `int` | `0` | — |
| `y` | `int` | `0` | — |
| `delta` | `float` | `0` | — |
| `alt` | `bool` | `false` | — |
| `control` | `bool` | `false` | — |
| `shift` | `bool` | `false` | — |
| `system` | `bool` | `false` | — |
| `session` | `string` | — | Active text-editing session ID (decimal string). |
| `unicode` | `int` | `0` | Unicode scalar for `TextEntered`. |
| `text` | `string` | — | UTF-8 pre-edit string for `TextPreedit`. |
| `preeditCaret` | `int` | `0` | UTF-8 byte offset for `TextPreedit`. |
| `composing` | `bool` | `false` | Composition state for `TextComposition`. |

Text editing requires the active session ID in `session` (stale sessions are ignored). A `TextEntered` event without a session reaches gameplay only when editing is not blocking input. Text, composition, key and focus events preserve injection order.

## JoystickAxisEvent

Payload returned by `Service.getJoystickAxisMoved`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `axis` | `sf.Joystick.Axis` | — | — |
| `position` | `float` | `0` | — |

## Input hint device

`Engine.Input.getInputHintDevice()` returns `Engine.InputHintDevice.KeyboardMouse`, `Gamepad` or `Touch`. This presentation state is independent of `isMouseInputMode()`, which distinguishes pointer interaction from keyboard/gamepad navigation.

- **Initial and connection updates:** A connected controller selects `Gamepad` on startup, and connecting a controller switches to `Gamepad` immediately. Disconnecting keeps `Gamepad` while another controller remains; disconnecting the last restores the platform default (`Touch` on mobile; `KeyboardMouse` on desktop, including desktop HarmonyOS).
- **Activity precedence:** Valid keyboard/mouse, controller or touch input switches to its device and takes precedence over connection changes in the same frame. Stick motion inside the dead zone, touch-generated mouse cancellation and events from disconnected controllers never change the hint device.

Read this value when resolving Tutorial text, help lists or other input prompts, and refresh visible text when it changes. Connecting a controller does not permanently lock presentation to `Gamepad`.

## Raw touch ownership

- **Primary finger selection:** The service never assumes raw touch ID `0`. When no accepted touches are tracked, the first `TouchBegan` inside the pointer viewport becomes the primary finger and drives all single-touch state, tap, position and delta queries. If the primary finger ends while another finger stays down, the secondary finger is not promoted; a new primary begins only after every tracked finger ends and a later valid `TouchBegan` arrives.
- **Two-finger cancel:** Starting a second accepted finger immediately cancels the active single-touch gesture and emits the synthetic right-mouse press and release used for menu cancellation. Secondary-finger movement or release cannot resume the cancelled gesture, become primary or emit a tap.
- **Explicit teardown (`cancelTouchGesture`):** Cancels the primary gesture and suppresses its remaining began, moved, ended and tap state until every finger in the physical gesture is released, after which the next valid `TouchBegan` starts normally.
- **Long press (`isTouchLongPressed`):** `isTouchLongPressed(handled = false)` fires once when a primary touch is held for at least `600` ms within the `8`-logical-pixel drag tolerance, and cannot fire again until a new physical gesture starts. `getTouchLongPressPosition()` returns the position in window-view coordinates (matching `getTouchTapPosition()`) or `nil` outside that frame.
- **Long-press consumption and cancellation:** Passing `true` to `isTouchLongPressed` consumes the event and suppresses the release tap; observing without consuming preserves the normal tap. Dragging, a second finger, focus loss, input capture, touch blocking, window recreation or `cancelTouchGesture()` cancels recognition.

## WebView input capture

An accepted `Engine.OpenWebView(url)` request captures game input independently of `blockInput()` and device locks. Input queries, actions, native UI and injected events stay suppressed while updates and rendering continue, and `unblockInput()` cannot dismiss it. After closing the WebView, held controls must be released before driving gameplay again.

## Platform Back cancellation

On Android and HarmonyOS, the system Back button or gesture cancels any active touch gesture and queues one Escape press-and-release pulse for the next input frame (multiple requests before that frame coalesce). Because the pulse matches `getCancelKeys()`, UI Cancel handling runs without exiting or backgrounding the runtime. Two-finger cancellation remains distinct and emits synthetic right-mouse input.

## Keyboard edge and repeat semantics

- **Edge deduplication:** Known keys and scancodes emit press and release events only when held state transitions. Repeated physical Down events (native auto-repeat or duplicate native/injected delivery) emit no extra press, unmatched or repeated Up events emit no release, and unknown keys never enter held state. Injected events use their valid scancode or derive missing scancode/key from whichever known field is supplied.
- **HarmonyOS external keyboards:** Physical keyboards forward every ordinary key mapped by SFML's Harmony key-code table as edges carrying scancode, resolved logical key and Alt/Control/Shift/System modifiers, following the same deduplication rules. Focus loss, backgrounding or host destruction releases all host-held keys. Physical-key forwarding is separate from text entry: ordinary Surface keys can emit both edges and text, IME-focused input uses the text path, and unsupported key codes stay with the host.
- **Default keys and repeat:** `getConfirmKeys()` includes Enter and Space; `getCancelKeys()` includes Escape. Held-key repeat applies to `isKeyTriggered` and `isActionTriggered` outside text editing.

## Default controller directions

Default Up, Down, Left and Right actions read only left-stick `X`/`Y` (dead zone `10`) and D-pad `PovX`/`PovY` (threshold `50`). For both `isActionTriggered` and `isActionHeld`, `|X| >= |Y|` selects horizontal and `|Y| > |X|` selects vertical (an exact 45-degree diagonal resolves as horizontal). Right-stick and trigger axes do not compete for this dominant direction, while custom joystick-axis action mappings evaluate their configured axis independently.

## Text editing sessions

An editing [TextBox](<Interactive and Declarative UI Controls.md#textbox>) consumes text, shortcuts and keyboard/gamepad actions so `getEnteredText()` stays empty for gameplay. Desktop pointer input stays available. Closing keystrokes do not forward to gameplay, and held keys remain blocked until released.

## InputCapture

`Engine.Input.captureInput()` returns a capture handle blocking normal input and UI interaction while updates, rendering and latent scheduling continue. `Engine.Input.isInputCaptured()` reports active captures as well as the post-release wait for held controls to be released.

- **Confirmation:** Disabled initially; enable with `handle:setConfirmEnabled(true)` and read with `handle:consumeConfirm()`. Only fresh Enter/Space, left-mouse or logical A/Cross presses and single-finger taps confirm (taps must start inside the pointer viewport after enabling and end on the same active handle; dragging or multi-touch cancels). Toggling enabled discards pending confirmation and in-progress taps.
- **Stacking and release:** Call `handle:release()` on cancellation or disposal (idempotent; handle destruction also releases). Only the newest active handle receives confirmation, with WebView taking priority. Releasing one handle keeps remaining captures active and blocks the closing frame and held controls until release.

## Input

`Engine.Input` exposes bound static singleton wrappers for the public `Engine.Service` methods below. Their Blueprint nodes take the same parameters and defaults with no `self` input — for example, `Engine.Input.getKeyDown(key)` uses the active input service directly, whereas `Engine.Service.getKeyDown(self, key)` takes an explicit `Service` receiver.

## Service

Manages per-frame polling, event injection, device blocking and action queries for an `sf.RenderWindow`.

- `update(window)` requires an `sf.RenderWindow`. Mouse and touch positions map pixels to world coordinates using that window's current view, and `setMousePosition(position)` maps world coordinates back to pixels.
- Native `onWindowRecreated(window)` requires an `sf::RenderWindow&` and resets the active input window after recreation. The native `setMousePosition(position, sf::WindowBase&)` overload accepts pixel coordinates directly.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `update` | `window: sf.RenderWindow` | — | — |
| `injectEvent` | `event: Engine.InjectedInputEvent` | — | — |
| `setUseInjectedMouseOnly` | `value: bool` | — | — |
| `getInputHintDevice` | — | `Engine.InputHintDevice` | Pure. Blueprint enum: `Enums.Engine.InputHintDevice`. |
| `isFocused` | — | `bool` | Pure. |
| `isFocusLost` | — | `bool` | Pure. |
| `isFocusGained` | — | `bool` | Pure. |
| `isKeyPressed` | — | `bool` | Pure. |
| `isKeyReleased` | — | `bool` | Pure. |
| `getKeyPressed` | `key: sf.Keyboard.Key`, `handled: bool`, `alt: bool`, `ctrl: bool`, `shift: bool`, `system: bool` | `bool` | — |
| `getScanPressed` | `scan: sf.Keyboard.Scancode`, `handled: bool`, `alt: bool`, `ctrl: bool`, `shift: bool`, `system: bool` | `bool` | — |
| `getKeyReleased` | `key: sf.Keyboard.Key`, `handled: bool`, `alt: bool`, `ctrl: bool`, `shift: bool`, `system: bool` | `bool` | — |
| `getScanReleased` | `scan: sf.Keyboard.Scancode`, `handled: bool`, `alt: bool`, `ctrl: bool`, `shift: bool`, `system: bool` | `bool` | — |
| `isMouseWheelScrolled` | — | `bool` | Pure. |
| `getMouseScrolledWheel` | — | `sf.Mouse.Wheel` | Pure. |
| `getMouseScrolledWheelDelta` | — | `float` | Pure. |
| `isMouseWheelPrecise` | — | `bool` | Pure. |
| `getMouseScrolledWheelPosition` | — | `sf.Vector2i` | Pure. |
| `isMouseButtonPressed` | — | `bool` | Pure. |
| `isMouseButtonReleased` | — | `bool` | Pure. |
| `getMouseButtonPressed` | `button: sf.Mouse.Button`, `handled: bool` | `bool` | — |
| `getMouseButtonReleased` | `button: sf.Mouse.Button`, `handled: bool` | `bool` | — |
| `isMouseMoved` | — | `bool` | Pure. |
| `getMousePosition` | — | `sf.Vector2i` | Pure. |
| `getMouseMovedDelta` | — | `sf.Vector2i` | Pure. |
| `setMousePosition` | `position: sf.Vector2i` | — | — |
| `isMouseEntered` | — | `bool` | Pure. |
| `isMouseLeft` | — | `bool` | Pure. |
| `isTouchBegan` | `handled: bool` | `bool` | — |
| `isTouchTap` | `handled: bool` | `bool` | — |
| `isTouchLongPressed` | `handled: bool = false` | `bool` | — |
| `getTouchLongPressPosition` | — | `sf.Vector2i` | Pure. |
| `isTouchEnded` | — | `bool` | Pure. |
| `isTouchMoved` | — | `bool` | Pure. |
| `isTouchActive` | — | `bool` | Pure. |
| `isTouchDragged` | — | `bool` | Pure. |
| `getTouchPosition` | — | `sf.Vector2i` | Pure. |
| `getTouchBeganPosition` | — | `sf.Vector2i` | Pure. |
| `getTouchTapPosition` | — | `sf.Vector2i` | Pure. |
| `getTouchEndedPosition` | — | `sf.Vector2i` | Pure. |
| `getTouchMovedDelta` | — | `sf.Vector2i` | Pure. |
| `isTouchTriggered` | `handled: bool` | `bool` | — |
| `isTouchBlocked` | — | `bool` | Pure. |
| `cancelTouchGesture` | — | — | — |
| `blockTouch` | — | — | — |
| `unblockTouch` | — | — | — |
| `isJoystickButtonPressed` | — | `bool` | Pure. |
| `isJoystickButtonReleased` | — | `bool` | Pure. |
| `isMouseInputMode` | — | `bool` | Pure. |
| `getJoystickButtonPressed` | `joystickId: int`, `button: int`, `handled: bool` | `bool` | — |
| `getJoystickButtonReleased` | `joystickId: int`, `button: int`, `handled: bool` | `bool` | — |
| `isJoystickAxisMoved` | — | `bool` | Pure. |
| `getJoystickAxisMoved` | `joystickId: int`, `handled: bool` | `Engine.JoystickAxisEvent` | — |
| `isJoystickConnected` | — | `bool` | Pure. |
| `isJoystickDisconnected` | — | `bool` | Pure. |
| `isKeyTriggered` | `key: sf.Keyboard.Key`, `alt: bool`, `ctrl: bool`, `shift: bool`, `system: bool`, `handled: bool`, `repeatDelay: float`, `repeatInterval: float` | `bool` | — |
| `isJoystickButtonDown` | `joystickId: int`, `button: int` | `bool` | Pure. |
| `isAnyJoystickButtonDown` | `button: int` | `bool` | Pure. |
| `isAnyJoystickButtonTriggered` | `button: int`, `handled: bool`, `repeatDelay: float`, `repeatInterval: float` | `bool` | — |
| `isActionTriggered` | `actionKeys: Engine.InputActionKey[]`, `handled: bool`, `repeatDelay: float`, `repeatInterval: float` | `bool` | — |
| `isActionHeld` | `actionKeys: Engine.InputActionKey[]` | `bool` | Pure. |
| `isMouseButtonTriggered` | `button: sf.Mouse.Button`, `handled: bool` | `bool` | — |
| `isMouseButtonDown` | `button: sf.Mouse.Button` | `bool` | Pure. |
| `getEnteredText` | — | `string` | Pure. |
| `isTextEntered` | — | `bool` | Pure. |
| `isKeyboardBlocked` | — | `bool` | Pure. |
| `isMouseBlocked` | — | `bool` | Pure. |
| `isJoystickBlocked` | — | `bool` | Pure. |
| `blockKeyboard` | — | — | — |
| `blockMouse` | — | — | — |
| `blockJoystick` | — | — | — |
| `unblockKeyboard` | — | — | — |
| `unblockMouse` | — | — | — |
| `unblockJoystick` | — | — | — |
| `blockInput` | — | — | — |
| `unblockInput` | — | — | — |
| `getConfirmKeys` | — | `Engine.InputActionKey[]` | Pure. |
| `getCancelKeys` | — | `Engine.InputActionKey[]` | Pure. |
| `getUpKeys` | — | `Engine.InputActionKey[]` | Pure. |
| `getDownKeys` | — | `Engine.InputActionKey[]` | Pure. |
| `getLeftKeys` | — | `Engine.InputActionKey[]` | Pure. |
| `getRightKeys` | — | `Engine.InputActionKey[]` | Pure. |
| `registerActionMapping` | `object: any`, `actionName: string`, `actionKeys: Engine.InputActionKey[]`, `callback: function`, `triggerOnHold: bool` | — | — |
| `unregisterActionMapping` | `object: any`, `actionName: string` | — | — |
