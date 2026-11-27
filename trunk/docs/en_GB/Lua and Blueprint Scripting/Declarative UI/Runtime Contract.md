# Declarative UI Runtime Contract

Generated Views construct and own the native UI object tree, while Controllers layer business behaviour, data observation and dynamic collections on top. This page defines the runtime lifecycle, ownership and disposal guarantees across Views, windows and controls.

## Create a View

```lua
local InventoryView = require("Internal.UI.Inventory")

local inventory = InventoryView.new()
inventory:mount(uiManager, logicalSize)
```

`View.new()` instantiates the JSON asset, wraps nested assets in their generated View classes and populates all typed references recursively without requiring a Controller. Construction does not mount the tree on screen — call `mount` or `attachTo` explicitly.

- **View members**: Each View exposes `assetKey`, `designSize: sf.Vector2f`, the native `instance: Engine.AssetInstance`, its typed `root`, `controls` (native system controls) and `assets` (typed child Views). Access a child control as `inventory.assets.ItemRow.controls.Label`; keys match exact asset-local names, so use bracket indexing for names that are not Lua identifiers.
- **Resource sharing**: Repeated references within one top-level View share the loaded asset definition and source texture, while each nested instance owns an independent View, native controls, texture rectangle and animation state (pass that child View to its Controller). A `WrapBox` shares the same resources when its count grows. Creating a new top-level View or rebuilding a preview snapshot allocates fresh resources.

## Bind business behaviour

A window module defines a private Controller and returns `Ui.DefineWindow(viewClass, Controller, nativeBase?)` (default `nativeBase`: `Engine.Canvas`). The returned UI inherits `nativeBase`, while the Controller (`UiController`) and generated View stay separate objects. Call `Window.new(...businessArgs):mount(uiManager)` to create and register the window; an empty Controller needs no `init` and displays the template as authored.

### Construction lifecycle

1. Create the native host and generated View, bind both to the Controller, and attach the View.
2. Call optional business `init(...)`. At this point `self.ui` is the typed View, `self.view` is `Engine.AssetInstance`, `self.root` is the native root and `self.host` is the window UI (which exposes `window.ui` and keeps the Controller private).
3. Run `prepare` to invoke `bind`, `refresh` and reflow.
4. Call optional `ready` (for business logic that depends on prepared layout) and apply the configured initial hidden state.

`Ui.DefineWindow` forwards Controller instance methods (`camelCase`), static helpers (`PascalCase`), `SCREAMING_SNAKE_CASE` constants and `Publish` onto the public window class while keeping construction hooks private (see [File layout](<Authoring Workflow.md#file-layout>)). Independent row and Scene Controllers use `Ui.Define(viewClass, definition, baseClass?)` and `Controller.new(model, ui?)`, where a supplied View must match the generated class and accepts at most one Controller.

### `Controller.windowOptions`

| Option group | Fields and behaviour |
|---|---|
| Placement and sizing | `position`, `centered`, `screen` (full-screen extent). Host size comes from the template unless full-screen behaviour is requested. |
| Initial interaction | `hidden`, `returnButton`, `focusable`. |
| Window bases | `frame` (default `"WindowFrame"`), `content` (default `"Content"`). |
| Selectable bases | `list`, `scroll`. Selection and hit bounds come from the ListView's native item layout. |
| Transitions | `transitionTarget` selects an animation target node. |

`windowOptions` configures host behaviour only; it never reproduces JSON hierarchy or evaluates business expressions.

## Observe model fields and bind callbacks

- **`Controller:watch(target, field, method, immediate?)`**: Monitors a table or userdata field via native `Class.monitor`. Pass an unbound Controller method, which receives `(controller, newValue, oldValue)`. By default (`immediate = true`), it runs immediately with `Class.MISSING` as `oldValue`; pass `false` to wait for the first change. The returned `stop()` cancels that subscription, and Controller disposal or row recycling cancels all owned watches automatically.
- **Shallow monitoring semantics**: `watch` observes only direct field writes, so mutations inside a table-valued field need separate watches or an explicit refresh event. Equality, `nil` writes and error propagation follow [Class utilities](<../../Getting Started/Ludork Lua Advanced.md#class-utilities>).
- **`Controller:bindCallback(method)`**: Wraps an unbound class method (e.g. `self:bindCallback(Controller.confirm)`) with a weak reference, supplying `self` and forwarding arguments and return values. Calls stop after Controller disposal, garbage collection or row recycling, and stale callbacks stay inert after row reuse.

## Preparation, child windows and mounting

- **Window skins**: Attaching a View to a window host preserves the frame's asset-defined `windowSkin` and `repeated` properties rather than overriding them with host defaults; explicit `applyWindowSkin()` calls apply the host's settings.
- **Window mounting**: `Window.new(...)` prepares and attaches its View automatically. `window:mount(uiManager)` registers the host once, `window:unmount()` detaches it while keeping the window reusable, and `window:dispose()` performs terminal release.
- **Child windows**: `self:createChild("AssetName", ChildWindow, ...businessArgs)` calls `ChildWindow.FromView(self.ui.assets.AssetName, ...)` (which also accepts an explicit child View). Both reuse the child View's controls and authored parent, bind an input host and prepare the child Controller. Position, size and stacking follow the generated layout, moving the child host updates its pane, and parent View disposal releases the child window automatically. The framework never discovers Controllers from JSON or resolves business dependencies implicitly.
- **Input-host ordering**: The input host is inserted immediately before its View in native child-update order so selectable hosts can capture list gestures before individual rows while keeping visual sibling order intact. Other touches (such as the return button and native child controls) pass through to the View.
- **Direct View vs. Controller mounting**: On a raw View, `View:prepare(logicalSize?)` reflows and returns the root, `View:mount(uiManager, logicalSize?)` prepares and registers once, and `View:attachTo(parent, logicalSize?)` prepares and reparents (without running Controller preparation). `Controller:attachTo` and `Controller:mount` prepare the Controller once and then call `View:attachPreparedTo(parent)` or `View:mountPrepared(uiManager)` so post-preparation geometry is preserved.
- **`Controller:prepare(logicalSize?)`**: Calls `bind` once per binding lifetime, subscribes declared `refreshEvents`, registers on the shared asset update channel (created on first preparation; requiring a module or creating a View alone does not subscribe), calls `refresh`, and prepares the View. Subsequent calls repeat `refresh` and reflow. Omitting `logicalSize` reuses the previous size from the same binding lifetime (used by row Controllers for dynamic layout and locale refreshes). Row reuse begins a new binding lifetime.

For content-driven resizing, call [`AssetInstance:reflowControl`](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#assetinstance>). Keep fixed layout in JSON and reserve Controllers for content measurement and mode switches.

## Create optional windows on first use

`Internal.UIBase.LazyWindow` defers creating optional popups until first display:

- **`LazyWindow.new(factory)`**: Stores `factory` without creating UI.
- **`handle:get()`**: Runs `factory` on first call, caches the window and returns that same instance afterwards.
- **`handle:peek()`**: Returns the cached window or `nil` without invoking `factory`. Use `peek()` for visibility checks, input blocking, closing, locale refreshes and player rebinding so unused UI is never created.
- **`handle:dispose()`**: Releases the created window (if any) and drops `factory`. Disposal is terminal and never triggers `factory`.

Inside `factory`, require the window module (which also defers loading its class definitions), call `Window.new(...)`, configure callbacks and focus, and mount onto the existing `UIManager`; the caller then invokes `handle:get():open(...)`. Factories read the current player, game instance and configuration when invoked. Owners may pre-create empty focus groups and neighbours, populating controls before mounting and activating the window.

Give each independent popup — including menu sub-windows — its own `LazyWindow` handle. `Window.new(...)` and `View.new()` always construct their complete trees (including authored child Views); lazy ownership only defers that call. Reopening reuses the existing window, Controller and View with normal refresh and animation behaviour.

## Dynamic collections

Use collections for model-dependent row counts, and bind fixed JSON children through `assets`.

- **Collection operations**: `view:createCollection(container, RowControllerClass)` (or `self:createCollection(...)` on a Controller) attaches one owned `UiCollection` to a Canvas or ListView and detaches authored preview children. `collection:add(model, logicalSize?)` pops an idle row or creates one, binds `model`, prepares and attaches its root, and returns the Controller. `collection.items` lists active Controllers in insertion order; call `collection:layout()` after a batch to update ListView positions.
- **Clearing, pooling and disposal**: `collection:clear()` empties `items`, hides and detaches roots, clears native callbacks and animations, cancels watches and subscriptions, and drops models and cached logical sizes. Each collection pools up to 64 idle Controller/View/control trees (not shared across containers or windows) and disposes excess rows. Repeated `clear()` calls are safe. `collection:dispose()` and parent View disposal terminally release active and idle rows. Never keep row references across `clear()` or dispose pooled rows directly.
- **Row Controller lifecycle**:
  - `init()` runs once per instance: capture template colours, fixed dimensions, static references and owned collections here; do not cache model-owned objects.
  - `bind()` runs for each bound model: validate data and register callbacks, watches and subscriptions.
  - `refresh()` must overwrite all model-dependent text, characters, colours, visibility and dimensions, including populated-to-empty transitions.
  - Business code calls only `clear`, `add` and `layout`; `releaseForReuse()` and `reuse(model)` are internal to the foundation.
- **Nested children**: Authored child Views keep their parents and restore root visibility/activity before refresh. Bound child Controllers lose their previous model and bindings on recycle — assign the child's new `model` and call `prepare()` inside the parent's `bind()`. Nested dynamic collections clear and recycle with their owner. Top-level windows keep their normal lifetime and are not pooled.

## Repeated template controls

An authored `Engine.WrapBox` owns one template and instantiates `count` independent native subtrees without flattening template descendants into the enclosing View's `controls` or `assets`. Access the 1-based root of each instance via `wrapBox:get(i)` (invalid indices raise an error) and verify concrete native types with `Class.isInstance` before narrowing `Engine.ControlBase`.

`getCount()` and `setCount(count)` update the non-negative instance count (`0` or an empty template leaves the control empty). Reflow, resize and spacing updates preserve existing instances; increasing `count` appends new instances, and decreasing releases only the removed tail. Each subtree maintains independent state, with `WrapBox` owning updates, input and disposal. Do not wrap a `WrapBox` in a `UiCollection`.

## Namespace and property values

Generated references respect the local `controls` and child `assets` boundaries described under [Create a View](<#create-a-view>). `Controller:requireAsset(name)` returns a generated child View, native `instance:requireControl(name)` and `instance:requireAsset(name)` return local native objects, and `instance:getNodeByName(name)` searches the subtree and rejects ambiguous names.

`AssetInstance:setProperty(name, propertyId, value)` and `UiController:setProperty` require the property's declared runtime type; see [AssetInstance](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#assetinstance>) for types and clearing optional values. Nesting JSON cannot override a child asset's internal properties or hierarchy.

## Animation playback

`hasAnimation(name, target?)`, `playAnimation(name, target?, onFinished?)` and `stopAnimation(name, target?)` exist on native instances, generated Views and Controllers for Global or asset-local node animations:

- **Playback**: Interpolation is linear and applies time `0` immediately. Natural completion holds the final frame and calls `onFinished` once.
- **Concurrency and stopping**: Independent targets play concurrently. Starting a new animation on the same target clears the previous effect and callback before starting at time `0`. `stopAnimation` clears only the matching animation's effect and pending callback (including a held final frame). Hiding or disposing the root clears all active effects and pending callbacks.
- **Missing animations and nesting**: A missing animation logs a warning with asset, target and name, returns `false` and invokes `onFinished` synchronously. Nested assets export only Global animations; a same-name parent animation targeting that nested node overrides only that instance (parent Global animations do not trigger the override, and child node animations are never exported).

## Disposal

- `View:dispose()` releases its bound Controller, nested Views and dynamic collections, stops animations, and unmounts/detaches the root. Use `View:own(child)` to attach an extra object exposing `dispose()`.
- `window:dispose()` and `Controller:dispose()` release the owned UI tree, clear update subscriptions, watches and native callbacks on generated controls, and are safe to call repeatedly. Controllers only need extra cleanup for external resources or callbacks.
- `UIManager` destruction clears callbacks bound to its lifetime (including on detached controls). `removeUI` preserves callbacks for a later `loadUI`, and loading or reparenting transfers ownership — removing a root from `UIManager` does not replace `dispose()`.

Disposed UI is terminal and must not be reused. [Lua hot reload](<../Lua Runtime and Modules.md#lua-hot-reload>) updates compatible Controller methods while keeping live UI state and subscriptions intact; it does not rerun `bind`, remount controls or rebuild the JSON tree.
