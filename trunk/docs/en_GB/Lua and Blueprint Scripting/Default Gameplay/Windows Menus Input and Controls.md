# Windows, Menus, Input and Controls

This page covers Default Gameplay's windows, HUD shortcuts, transition animations, localisation refreshes, Floor Teleporter previews and input/focus behaviour. To build custom UI assets and Controllers, see [Declarative UI Authoring Workflow](<../Declarative UI/Authoring Workflow.md>).

## Windows and menus

Default windows follow the [Declarative UI Runtime Contract](<../Declarative UI/Runtime Contract.md>): each screen owns one top-level asset with nested sub-panes, sibling input hosts reuse nested lists and scroll boxes, menus keep stable command keys separate from display text, and overflowing lists use [`Engine.ScrollBox`](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#scrollbox>).

- **Eager vs. lazy construction**: Map entry creates the player HUD and Region Title immediately. The title's Save/Load and Config windows and all optional map windows (Menu, Item, Equip, shops, Shop Records, Save/Load, Config, Enemy Book, Encyclopedia, Floor Teleporter, Player Name and Message) use separate [`LazyWindow` handles](<../Declarative UI/Runtime Contract.md#create-optional-windows-on-first-use>) and construct on first display (Message constructs on the first message or choice). Opening Menu does not instantiate unused sub-windows.
- **In-game menu**: Lists **Items**, **Equipments**, **Files**, **Config** and **Return to Title**. `WindowMenu.new(player, windows, onExit)` runs `onExit` after the closing animation while the scene owns destination routing and assigns one stacking layer to the menu and its sub-windows. Opening **Config** overlays the menu and deactivates the command list; closing returns focus to the menu.
- **Templates and preview placeholders**: Default assets include `#TAB` and `#ITEM` preview children. Enemy Book and Encyclopedia use [`CharacterView`](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#characterview>) with authored cell sizes while Controllers measure localised labels and special tags. The title's `WindowCommand` binds rows in `Parts/Title/CommandWindow` (placed by `Title`) following the [title scene's platform rules](<New Game Title and Scene Flow.md#new-game-title-and-scene-flow>).

### Window transitions

Top-level window transitions interpolate linearly (default `0.2` seconds), while internal panes and tabs switch immediately. Input locks during playback, close callbacks run after `FadeOut` hides the root, and initialisation/disposal hide immediately. Related `FadeOut` transitions may run in parallel; using an inventory item triggers its callback once the item window hides, and an open menu then closes and restores movement once no blocking window is open.

| UI | Timeline behaviour |
|---|---|
| Default windows and Config | `FadeIn`: centre Scale `0.5` to `1` plus colour; `FadeOut`: colour only. |
| Title command | `FadeIn`: local Y `80` to `0` plus colour. Opening Config or Save/Load leaves it visible. |
| Map Menu | `FadeIn`/`FadeOut`: local X `-192` to `0` and back, plus colour. |
| Item and Equip | Menu animations only. Save/Load supports both default and Menu pairs; Config always uses default. |
| Message | `Panel` colour only. |
| Game Over | Colour-only `FadeIn` (`0.5` s) and `FadeOut` (`1` s). Scene switching waits for `FadeOut`, which holds its transparent final frame until disposal. |
| Region Title | `Display` holds colour for `1` s, then fades over `1` s. |

Pass `Enums.WindowTransitionProfile.DEFAULT` (`"Default"`) or `.MENU` (`"Menu"`) when opening Item, Equip or Save/Load windows.

### Selection, chrome, touch and localisation

- **Selection and scroll**: Focusing a selectable list (on open, pane switch or return to parent) scrolls the active selection into view without moving the mouse cursor. Starting a new open session on Menu, Item, Equip, shops, Config or Enemy Book resets selectors to `0` (or `nil` if empty) and scrolls to the top; focus switches and in-place refreshes within a session keep the selection.
- **Touch and Web lists**: On mobile and Web, tapping an unselected row selects it without confirming, tapping the selected row confirms it, and dragging scrolls without confirming (applies to title commands, Save/Load slots and all selectable windows; Web also supports mouse selection — see [Web input](<../Global and Core Modules/Core Modules/Engine/Input and Services.md#web-input>)). On the title screen, open Config or Save/Load windows prevent the underlying command window from receiving any part of the same touch gesture.
- **Window return button**: `Parts/Shared/WindowChrome` places a `16 × 16` return button on the top-left border (outside `Content`). `WindowBase` binds it for windows setting `windowOptions.returnButton = true` and implementing `Controller:onReturn()`, playing System `CancelSE` once (other Cancel sources must not duplicate it). Multi-pane windows show it only on the active interactive pane; hidden, inactive or suppressed windows hide it.
- **Gamepad hints and pause mark**: `WindowChrome` also places an `Engine.GamepadHintBar` on the bottom border, displaying up to three `Engine.GamepadHint` entries when a gamepad is connected (`setGamepadHints`, `setGamepadHintEnabled`, `getGamepadHintCount`, `isGamepadConnected`, `setOnGamepadHintTriggered`; see [GamepadHintBar](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#gamepadhintbar>)). `Parts/Shared/WindowPauseMark` defines pause-mark bounds while `WindowBase` drives its animation and visibility.
- **Static vs. dynamic rows**: Static command rows store `localeKey` and dynamic rows store literal `text` (never both), letting language switches refresh controls in place without losing callbacks or cursor state.
- **Live locale refreshes**: `SceneMap` owns one `LocaleChanged` subscription for long-lived map windows. Active dialogue keeps its raw speaker, message/choices, explicit `localeArgs` and snapshots of node-local and instance variables so template localisation and string `localeArgs` rerun in place; Item/Equip details, shops, attribute shops, Enemy Book, `WindowEnemyEncyclopedia`, `WindowFloorTeleporter`, title commands, Menu, Save/Load tabs and visible Region Titles refresh in place while preserving selection, mode, focus and previews. Hidden windows refresh on next open; Config, save-slot and HUD controllers manage their own subscriptions.

### Shops and dialogue

- **Item shops**: Load `Enums.ShopMode` (`BUY = "buy"`, `SELL = "sell"`) for `WindowShop:setMode(mode)`. The two-column item list stays focused while **Q**/**E**, **LB**/**RB**, mouse or touch switches tabs (resetting cursor and scroll; transactions preserve valid selections). Cells show icons and owned counts; details show localised name, description, `Price` label and right-aligned price. Selling pays `floor(price / 2)` GOLD and hides zero-price items. Buy-only shops hide tabs and centre remaining panes. Cancel closes the shop, restores movement and blocks map input for one frame.
- **Attribute shops**: Expose snapshots via `getOffers()` and validate purchases via `purchaseAttribute(key)`. A scalar price reference is shared across offers, whereas an array assigns per-offer prices; each purchase increments its price.
- **Dialogue (`WindowMessage`)**: Uses a full-screen Canvas and transparent `FunctionalImage` to consume one click/tap for plain-message confirmation; choice mode (up to four options without a ScrollBox) disables that layer so blank-space clicks cannot pick a choice. `SceneMap` blocks map input only while dialogue is active. Dialogue, tutorial and window APIs return `Engine.AsyncOperation` (see [Lua waiting and results](<../Node Functions/Scene.md#lua-waiting-and-results>)).

### Floor Teleporter and static previews

- **Eligibility and single-map mode**: Lists maps in the current region that have recorded telepoints (maps without a region have none). With zero eligible maps, it plays the buzzer and refuses to open even when the player owns the item (without locking movement, taking focus or building previews). With one map, it opens directly on its first telepoint (requiring explicit confirmation; Cancel closes).
- **Multi-map mode**: With multiple maps, it opens the floor list with the current map selected and scrolled into view (or the first entry if absent), toggling the left pane between floor and telepoint lists (telepoints start at entry 1; Cancel returns to floors). Opening resolves the initial mode and selection before rendering its single preview.
- **Scrolling and naming**: Long map and telepoint names use [ListView child scrolling](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#listview>) inside fixed selection rectangles without shifting the right-side preview. A composite-world manifest appears as one floor with children at world offsets. Labels resolve recorded tags via `LOC` (falling back to localised `POINT` numbers for blank/auto tags, or the raw tag if untranslated).
- **Discovery and stair pairing**: Set each stair Blueprint's `stairDirection` for opposite-direction matching; telepoints record on stair transfers or `Teleporter:goToMap` even before acquiring the Teleporter item (opening the window or approaching stairs does not record). See [Teleporter](<../Source Classes/Teleporter.md>) and [Saves](<Runtime Data Configuration and Saves.md#saves>).
- **Preview rendering**: Previews update on keyboard, mouse or touch focus at fixed scale, clamped to map/manifest bounds and centring axes smaller than the viewport. They render from [static Actor display records](<Runtime Data Configuration and Saves.md#static-actor-display-data>) with material time fixed at `0`, excluding gameplay-script-only visuals.
- **Preview persistence and cache invalidation**: Destroyed tags and their children are filtered out before creating records or textures; added, moved and terrain edits plus declarative `childActorComp` children restore without running callbacks, and ordinary maps honour region hiding. `ConditionalActor` visibility evaluates against live variables without subscriptions (variable edits invalidate cached textures, and reopening clears preview models, textures and class templates).

### Player HUD shortcuts

The player HUD mounts a native `WrapBox` `8` logical pixels below the keys, flowing available shortcut buttons left to right without empty slots and wrapping when width runs out:

1. **Enemy Book** (when the player owns `Item.EnemyBook`; uses the item's icon).
2. **Floor Teleporter** (when the player owns `Item.Teleport`; uses the item's icon).
3. **Visited shops** (when `GameInstance` has at least one shop record; uses `Assets/System/mting-495.png` and obeys [Visited shops](<#visited-shops>) opening rules, including Teleporter ownership).

Icons are `32 × 32` with an `8`-pixel gap; mobile and Web expand each button's hit area to `40 × 40` via the [touch-hit bounds API](<../Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#touch-capture>) without scaling the image. Buttons never take keyboard focus or replace hotkeys.

- **Binding and event-driven refreshes**: `Source.Windows.HUDPlayerAttr.new(instance, openMenuCallback?, shortcutCallbacks?)` binds to `GameInstance` (with optional `enemyBook`, `floorTeleporter` and `shopRecords` callbacks wired by `SceneMap`). Call `setInstance(instance)` after loading or switching the primary player. Shortcuts rebuild only initially, on rebind, on `PlayerChanged` inventory events for the current player, and on [shop-record changes](<Runtime Data Configuration and Saves.md#saves>) for the current instance (never polled per frame).
- **HUD event subscriptions**: Subscribes to `LocaleChanged`, `AbilitySystemChanged` and `PlayerChanged` (`Enums.EventKey`), ignores events whose `owner` is another battler, and updates only changed controls. Payloads use `Enums.AbilitySystemChangeKind` (`Attribute`, `State`) or `Enums.PlayerChangeKind` (`Inventory`, `Name`, `Map`) under `Source.Configs.EventChangePayload`. Owned subscriptions and callbacks release with the HUD.

### Visited shops

- **Opening**: Releasing **V** opens `WindowShopRecords` when shop recording is enabled, the player owns the Teleporter item, at least one shop is recorded and the scene allows opening a window.
- **List display**: Shows records across all regions as single-column rows displaying localised `mapName` (or `worldName`), a space and the localised recorded tag, with in-row scrolling for long text. Reopening resets selection to row 1 and top scroll; language switches update labels without resetting selection or scroll.
- **Recording**: Both item and attribute shops record successful openings when opened from an Actor with a map identity (see [Shop Actors](<../Node Functions/Scene.md#shop-actors>) and [Saves](<Runtime Data Configuration and Saves.md#saves>)).
- **Remote execution**: Confirming a row closes the list and runs that shop's complete `onCollision` event with the current player as `other` while keeping the player on the current map. The shop Actor constructs independently from its saved map placement (`BPClassVarChanged` and world coordinates) without loading the target map scene, mutating shared class defaults or running `onCreate` — shop graphs must rely on configured fields and live game variables rather than target-map attachment or `onCreate` state. Dialogue, conditions and post-shop logic execute normally.
- **Closing and missing records**: Closing the shop returns directly to map play without reopening the list, and cancelling the list restores movement. A missing map or tag plays the buzzer while leaving the list and saved record intact (delete obsolete entries with `removeShop`). Remote openings never re-record the shop under the player's current map.

### Map introduction

`Map_01` places an invisible `BP_MapIntroduction` Actor that runs introductory guides in order after the map transition, then records and destroys itself.

- **Configuration**: `Source.Configs.Tutorial` maps tutorial keys to `{ rect: sf.IntRect, text: string }` using fixed logical screen coordinates (update rectangles if UI layout changes). `BP_MapIntroduction` passes its ordered keys through `ForEach` to `ShowTutorial`; the scene component renders whatever keys are requested without hard-coding map names or tags (see [Tutorials](<../Node Functions/Scene.md#tutorials>)).
- **Presentation**: The spotlight contracts to `rect` over `0.25` seconds while blocking input, dimming the rest of the screen (including HUD and margins) with a `40%`-opaque grey curtain, flashes the border once, and then enables [confirmation](<../Global and Core Modules/Core Modules/Engine/Input and Services.md#inputcapture>). Explanations render in size-12 outlined localised text (refreshing on language change), wrapping to screen width first, anchoring left-aligned at the bottom-middle of `rect`, shifting left to stay inside the right edge, and flipping above top-middle if space below is insufficient.

### Player name window

The NPC beside `Map_01`'s starting point opens `WindowPlayerName` and pauses map input until closed. Use **Tab** / **Shift+Tab** and directional keys to switch between the text input and **Confirm** (which stays visible across all devices).

- **Mouse and touch**: Activate the input or **Confirm** directly; submitting the mobile/Web system input dialog updates the draft without closing the window.
- **Keyboard**: **Enter** / **Space** applies the name when **Confirm** has focus, or starts editing otherwise. **Escape** exits editing first, and a second press discards the draft and closes the window.
- **Gamepad**: **A** starts editing (even while **Confirm** is focused). Holding **X** for one second (`gamepadButton = "X"`, `gamepadLongPress = true` on the button asset, without populating `GamepadHintBar`; see [Button](<../Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#button>)) applies the name regardless of focus. **B** (or the return button) exits editing on first press and discards/closes on the second.
- **Validation and HUD**: Invalid names disable **Confirm** (see [Source.MapActors.Player](<../Source Classes/Player.md>)). The HUD displays the confirmed name above the player's level, truncated to fit.

### Region-aware map previews

When `GameMap.HideDisconnectedRegions` is enabled, Enemy Book filters out Actors hidden by the player's current region before grouping and deduplicating. Floor Teleporter previews apply the same visibility mask using the player's cell on the current floor and the selected destination cell on other floors; changing destination regions or terrain invalidates the preview cache. Region hiding is visual only and never modifies telepoint records or destinations (see [Disconnected regions](<../Global and Core Modules/Global APIs/GameMap.md#disconnected-regions>)).

## Input

Runtime input comes from the C++ input service rather than editor state (embedded Windows play forwards compatible input over the runtime protocol; standalone builds read the native window directly).

- **Hotkeys**: `Source.Configs.HotKey` binds native keys to a `Scene`, a `Filter` and optional `FunctionWhenPressed` / `FunctionWhenReleased` callbacks receiving the scene instance.
- **Keyboard and gamepad actions**: Physical **Enter** and **Space** map to Confirm and **Escape** maps to Cancel (press and release are edge events). Controllers use logical buttons `Engine.JoystickButton.getA()` through `getShare()`, where **A**/**Cross** is Confirm and **B**/**Circle** is Cancel (see [Engine: Input and Services](<../Global and Core Modules/Core Modules/Engine/Input and Services.md>)).
- **Directional input**: Movement and menu navigation read only left-stick `X`/`Y` and D-pad `PovX`/`PovY` (right-stick and trigger axes cannot steal direction). Horizontal wins whenever `|X| >= |Y|` (exact 45-degree input resolves horizontally), and vertical wins otherwise, for both stepped navigation and held movement.
- **Mobile and HarmonyOS gestures**: On Android and HarmonyOS, the system Back button/gesture emits one Cancel/Escape pulse instead of closing or backgrounding the app — separate from HarmonyOS external-keyboard forwarding (which emits SFML scancode edges, resolvable logical-key edges and modifier state) and separate from two-finger touch (a right-click cancellation gesture). Both system Back and two-finger touch reach the focused window's `onReturn()`, though only two-finger touch emits right-mouse input.
- **Window cancellation**: `WindowSelectable` owns default Cancel-key and right-click closing; derived windows pass unhandled input up to it so each edge triggers one `CancelSE` and one close transition.

## Focus and navigation

Define an explicit focus order for keyboard and controller navigation, and ensure disabled or hidden controls never stay focused.

## Configuration controls

- **Config tabs and navigation**: Opens on **Graphics** with row 1 focused. **Q**/**E**, **LB**/**RB**, mouse hover and touch tap switch tabs with `CursorSE`. Each tab keeps its cursor and scroll within an open session and across locale refreshes; reopening resets all tabs. See [Game Settings Reference](<Game Settings Reference.md>) for option meanings.
- **DropBox controls**: Switching tabs collapses an open DropBox silently; Cancel and the return button collapse it first and close Config on the next press. An expanded DropBox overlays rows, scrolls internally when height-limited, captures input until closed and uses preview-then-confirm on mobile/Web touch (see [Interactive and Declarative UI Controls](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md>)).
- **Mobile and Web Sliders**: Tapping a Slider sets its value on release. Once dragging passes the drag threshold, horizontal drags belong to the Slider while vertical or tied drags scroll the Config list (when overflowing), keeping gesture ownership locked until release.
- **Save/Load window**: Shares one slot list across Load and Save tabs, preserving cursor, scroll and detail when switching tabs. Reopening defaults to Load on the newest save (or slot `0` if none exists), whereas map hotkey **S** opens in Save mode. Cancel closes the window; title load-only mode hides the tabs and centres the remaining panes.

For low-level UI and input APIs, start with [GlobalFunctions.Container](<../Node Functions/Container.md>). For text localisation keys, see [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md>).
