# Windows, Menus, Input and Controls

This page describes the default game's windows and input behaviour. To create your own UI, start with [Declarative UI Authoring Workflow](<../Declarative UI/Authoring Workflow.md>).

## Windows and menus

Window views follow the [Declarative UI Runtime Contract](<../Declarative UI/Runtime Contract.md>). Menus keep stable command identifiers separate from displayed text. Long descriptions use the existing wrapping and formatting helpers.

The title's Save/Load and Config windows and the map's optional windows use separate [`LazyWindow` handles](<../Declarative UI/Runtime Contract.md#create-optional-windows-on-first-use>). Map entry creates HUD and Region Title directly. Menu, Item, Equip, shops, Shop Records, Save/Load, Config, Enemy Book, Encyclopedia, Floor Teleporter and Player Name are created on first display; Message is created on the first message or selection. Opening Menu leaves its unused sub-windows uncreated.

The in-game menu lists Items, Equipments, Files, Config and Return to Title. `WindowMenu.new(player, windows, onExit)` receives the scene's exit action and invokes it after the menu's closing animation. The scene owns the destination and assigns one stacking layer to the menu and its independent sub-windows. Selecting Config opens the shared Config window over the menu and deactivates the command list. Closing the Config window returns focus to the menu.

Each default screen has one top-level asset; multi-pane screens nest their panes under that root. Sibling input hosts reuse the nested lists and scroll boxes.

Top-level window transitions are linear and default to 0.2 seconds. Internal panes and tabs switch immediately. Input is disabled during playback, and close results run after `FadeOut` hides the root. Initialisation and disposal hide immediately.

| UI | Timeline behaviour |
|---|---|
| Default windows and Config | `FadeIn`: centre Scale `0.5` to `1` plus colour; `FadeOut`: colour only. |
| Title command | `FadeIn`: local Y `80` to `0` plus colour. Opening Config or Save/Load does not hide it. |
| Map Menu | `FadeIn`/`FadeOut`: local X `-192` to `0` and back, plus colour. |
| Item and Equip | Menu animations only. Save/Load has both default and Menu pairs, while Config always uses the default pair. |
| Message | `Panel` colour only. |
| Game Over | Colour-only `FadeIn` (0.5 seconds) and `FadeOut` (1 second). Scene switching waits for `FadeOut`, which holds its transparent final frame until scene disposal. |
| Region Title | `Display` holds colour for one second, then fades for one second. |

Related `FadeOut` transitions may run in parallel. Using an inventory item runs its completion callback after the item window is hidden; an open menu then closes and restores movement once no blocking window remains.

Default assets include `#TAB` and `#ITEM` preview children. Enemy Book and Encyclopedia use [`CharacterView`](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#characterview>) with authored cell sizes; Controllers measure localised names and special tags within those areas.

The title's `WindowCommand` binds the four existing rows in `Parts/Title/CommandWindow`; its parent `Title` asset owns placement.

When a selectable list gains focus, the current selection scrolls into view while the mouse pointer stays in place. This also applies when opening a window, switching panes or returning to a parent list.

On mobile, tapping a different row moves the selection cursor without confirming. Tapping the currently selected row confirms it. Dragging the list scrolls without confirming a row. This applies to the title command window and the shared Save/Load slot list as well as other selectable windows.

On the title screen, an open Config or Save/Load window prevents the underlying command window from receiving any part of the same touch gesture.

The shared `Parts/Shared/WindowChrome` asset provides a 16 × 16 return button on the top-left border, outside `Content`. `WindowBase` binds it, and cancellable windows enable it with `windowOptions.returnButton = true` and implement `Controller:onReturn()` with the same state-aware step used by other Cancel inputs. That shared return step plays the System CancelSE once, so a second Cancel source must not play it again. Multi-pane groups expose the button only on the active interactive pane. Hidden, inactive or suppressed windows do not expose it.

`WindowChrome` also places an `Engine.GamepadHintBar` on the bottom border, outside `Content`, and owns its text style. It stays empty unless a gamepad is connected and the window supplies hints. A window sets up to three `Engine.GamepadHint` entries with `setGamepadHints`, toggles one with `setGamepadHintEnabled`, reads `getGamepadHintCount` and `isGamepadConnected`, and handles completions through `setOnGamepadHintTriggered`. The separate `Parts/Shared/WindowPauseMark` asset fixes pause-mark size and placement; `WindowBase` supplies its skin animation and visibility. See [GamepadHintBar](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md#gamepadhintbar>) for rendering and the one-second long-press.

Static command rows store a `localeKey`, while dynamic rows store literal `text`. A row must never define both. The row resolves `localeKey` when it refreshes, so a language change can update the existing controls without replacing their callbacks or cursor state. The title commands, in-game menu, save/load tabs, shop tabs and details, and a visible map-region title update immediately when the runtime language changes.

Whenever a reusable selectable window begins a new open session, its selectors start at zero and scroll to the top. An empty selector uses `nil`. This behaviour applies to the in-game menu, item and equipment windows, shops, Config and Enemy Book. Focus changes and in-place refreshes within that session preserve the selection.

Load `Enums.ShopMode` for the public `WindowShop:setMode(mode)` values `BUY = "buy"` and `SELL = "sell"`. Item, Equip and Save/Load opening calls use `Enums.WindowTransitionProfile.DEFAULT` (`"Default"`) or `.MENU` (`"Menu"`) as their transition profile.

The shop keeps its two-column item list focused while Q/E, LB/RB, mouse or touch switches Buy/Sell. Switching tabs resets the cursor and scroll, while a transaction refresh preserves a still-valid selection. Buy and Sell cells show the icon and current owned count, and details show the localised name, description, a localised `Price` label and the right-aligned price. Selling grants `floor(price / 2)` GOLD and omits zero-price results. Buy-only shops hide the tabs and centre the remaining panes. Cancel closes the shop. Closing restores the prior movement state and blocks map input for one frame.

Overflowing lists use [`Engine.ScrollBox`](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#scrollbox>) for clipping, scrolling and keeping the selection visible.

The message window uses a full-screen Canvas and a transparent `FunctionalImage` to consume one click or tap for plain-message confirmation. Choice mode disables that layer, so blank-space clicks cannot select an option. Its maximum of four options fits directly in the panel without a ScrollBox. `SceneMap` only blocks map input while dialogue is active.

`SceneMap` owns one `LocaleChanged` subscription for its long-lived map windows. Active dialogue retains the raw speaker, message or options, explicit `localeArgs`, and snapshots of node-local and instance variables. It reruns template localisation and localises string `localeArgs`. It then applies those arguments and the two variable snapshots before updating the existing `WindowMessage` controls. Item and equipment details, shop tabs and details, attribute shops, Enemy Book, `WindowEnemyEncyclopedia`, and Teleporter (`WindowFloorTeleporter`) likewise refresh localised text in place while preserving their current selection, mode, focus and preview state. Hidden windows resolve the active language when they next open, while Config, save-slot and player-HUD controllers keep their own subscriptions. The player HUD also subscribes to `AbilitySystemChanged` and `PlayerChanged` from `Enums.EventKey`, ignores payloads whose `owner` is another battler, and refreshes only the affected controls when a signature of the current player changes. Change payloads use `Enums.AbilitySystemChangeKind` (`Attribute`, `State`) or `Enums.PlayerChangeKind` (`Inventory`, `Name`, `Map`); their shared record is declared as `Source.Configs.EventChangePayload`.

The Teleporter lists maps in the current region that have recorded telepoints. If no maps qualify, it plays the buzzer sound and refuses to open even when the player owns the item, without disabling movement, taking focus or generating a preview. With one map, it opens directly at that map's first telepoint; cancelling closes the window. Even a single telepoint still requires confirmation. With multiple maps, it opens the floor list with the current map selected and scrolled into view, falling back to the first entry when the current map is absent. Its left slot then alternates between the floor and telepoint lists, and cancelling telepoint selection restores the floors. Telepoint selection starts at the first entry. Opening resolves the final selection and list mode before generating its single preview. Long map and telepoint names use [ListView child scrolling](<../Global and Core Modules/Core Modules/Engine/Layout and Image Controls.md#listview>) within their cells while the selection rectangle stays fixed. The telepoint list scrolls without moving the right-side preview. A composite manifest remains one floor identity, with child maps composed at world offsets.

Set each stair Blueprint's `stairDirection` so transfers can find an opposite-direction arrival. See [Teleporter](<../Source Classes/Teleporter.md>) for selection and recording rules.

Telepoints are discovered through stair transfers or `Teleporter:goToMap`, even before the Teleporter item is acquired. Opening the window or approaching stairs does not record a visit. A map without a configured region has no eligible floors, so the window cannot open. See [Teleporter](<../Source Classes/Teleporter.md>) for endpoint recording and [Runtime Data, Configuration and Saves](<Runtime Data Configuration and Saves.md#saves>) for record structure and deduplication.

Telepoint labels resolve the recorded tag through `LOC`. Empty or automatically generated tags use the localised `POINT` number format, and missing translations retain the raw tag.

Keyboard, mouse and touch selection update the destination and preview before confirmation. The preview keeps a fixed scale and stays within map bounds, centring axes smaller than the view. Composite previews use the full manifest bounds.

Floor previews use [static Actor display records](<Runtime Data Configuration and Saves.md#static-actor-display-data>), preserving configured visuals and material parameters with material time fixed at `0`. They do not run gameplay, so appearances created only by gameplay scripts are absent.

Destroyed tags and their child trees are filtered before display records or textures are created. Added and moved records and terrain destruction are restored; declarative `childActorComp` children retain their offsets, tags and ancestor visibility without running component callbacks. Ordinary maps retain their region-hiding rules, and composite children retain world offsets.

The static records evaluate [ConditionalActor](<../Source Classes/ConditionalActor.md>) visibility against current game variables, including on other floors and composite-world children, without subscriptions on preview objects. Changes to variables used by cached previews invalidate their textures; the next draw reevaluates the cached records. Reopening the window clears its preview models, textures and class display templates.

Attribute shops expose display snapshots through `getOffers()` and validate purchases through `purchaseAttribute(key)`. A scalar price reference is shared by all offers, while an array sets per-offer prices. A purchase increments the corresponding price.

Dialogue, tutorial and window APIs return `Engine.AsyncOperation`. See [Scene operations](<../Node Functions/Scene.md#lua-waiting-and-results>) for completion results and [Lua asynchronous tasks](<../../Getting Started/Ludork Lua Advanced.md>) for sequential waiting.

### Visited shops

Release **V** to open `WindowShopRecords` when shop recording is enabled, the current player owns the Teleporter item, at least one shop is recorded, and the scene allows a window to open. The list includes records from every region. Its single-column children are text controls displaying the map's localised `mapName` (or a world's `worldName`), a space, and the localised recorded tag. Long text scrolls within its row. Reopening selects the first row and scrolls to the top; changing language refreshes the text without resetting selection or scroll.

Both item and attribute shops record successful openings. [Shop nodes](<../Node Functions/Scene.md#shop-actors>) identify the shop Actor; an opening without a map identity remains usable but creates no record. The record format, script methods and recording switch are defined in [Runtime Data, Configuration and Saves](<Runtime Data Configuration and Saves.md#saves>).

Confirming a row closes the list and executes the recorded shop's complete `onCollision` event with the current player as `other`. The player stays on the current map. The shop Actor is constructed independently from its original map placement, including that placement's `BPClassVarChanged`; a world placement retains its world coordinates. The target map is not loaded as a scene, shared class defaults are unchanged, and `onCreate` is not run. Shop graphs therefore use their configured fields and current game variables rather than relying on target-map attachment or state initialised only by `onCreate`. Dialogue, conditions and logic after the shop closes all remain part of the event.

Closing the shop returns to map play, without reopening the list. Cancelling the list restores the previous movement state. A missing map or placement tag plays the buzzer and leaves the list and stored record intact; remove an obsolete entry with `removeShop`. Remote openings do not add the shop under the player's current map.

### Map introduction

`Map_01` uses the invisible `BP_MapIntroduction` Actor to introduce the HUD menu portrait and representative Actors after the map transition. It runs the guides in order, then records and destroys itself.

`Source.Configs.Tutorial` maps each key to `{ rect: sf.IntRect, text: string }`. Rectangles use fixed logical screen coordinates and do not follow Actors. The `BP_MapIntroduction` graph lists the tutorial keys in its `ForEach` input and calls `ShowTutorial` for each key. The scene component reads the requested configuration and manages presentation; it does not choose tutorials by map name or Actor tags. Update the rectangles when changing the layout.

The opening contracts to the rectangle in 0.25 seconds, with interaction blocked. A 40%-opaque grey curtain covers the rest of the game screen, including the HUD and margins. The border flashes once, and [confirmation](<../Global and Core Modules/Core Modules/Engine/Input and Services.md#inputcapture>) becomes available after contraction.

Explanations use size 12, outlined text and short localised sentences, refreshed when the language changes. Left-aligned text starts at the rectangle's bottom-middle, shifts left to stay within the right edge, and moves above the top-middle if it cannot fit below. Text wider than the screen wraps first. See [tutorial nodes](<../Node Functions/Scene.md#tutorials>) for invocation.

### Player name window

The NPC beside the starting point in `Map_01` opens `WindowPlayerName` and pauses map input until it closes. Activate the input to edit with the Confirm action. Tab/Shift+Tab and directional actions move between the input and Confirm.

Confirm stays visible for every input device. Mouse and touch activate the button. Keyboard Enter/Space applies the name when Confirm has focus, otherwise starts editing. The asset binds Confirm to a one-second X long-press (`gamepadButton = "X"`, `gamepadLongPress = true`) and does not populate the window hint bar. A only starts editing, including while Confirm has focus. Holding X applies the name from any focus in the active window. See [Button](<../Global and Core Modules/Core Modules/Engine/Runtime Values and Functional Controls.md#button>) for the empty-texture and hint appearance. The return button or Cancel discards the draft; while editing, Cancel (B or Escape) first ends editing and a second Cancel closes the window. Completing the mobile input dialog only updates the draft. Invalid names disable Confirm; see [Source.MapActors.Player](<../Source Classes/Player.md>) for the validation rules. The HUD shows the confirmed name above the level, shortened to fit.

### Region-aware map previews

When `GameMap.HideDisconnectedRegions` is enabled, Enemy Book excludes Actors hidden by the current region before grouping and deduplication. Floor Teleporter previews follow the same rule: the current floor uses the player's position, and another floor uses the selected destination cell. Selecting a different destination region or changing terrain invalidates the preview cache. Region hiding affects presentation only; telepoint records and transfer destinations are unchanged. See [Global.GameMap](<../Global and Core Modules/Global APIs/GameMap.md#disconnected-regions>).

## Input

`Source.Configs.HotKey` maps native keys to a `Scene`, a `Filter` and optional `FunctionWhenPressed` / `FunctionWhenReleased` callbacks. Each callback receives the scene instance.

Runtime input comes from the C++ input service. Use documented key, mouse, controller and injected-input APIs rather than reading editor state. Embedded Windows mode forwards compatible input through the runtime protocol, while standalone games use the native window directly.

On Android and HarmonyOS, the host converts the system Back button or gesture into one Cancel/Escape pulse instead of closing or backgrounding the app. This conversion remains separate from HarmonyOS external-keyboard forwarding, which sends ordinary SFML-mapped keys as scancode edges, adds logical-key edges where they can be resolved, and preserves their modifier state. Two-finger touch remains a separate right-click cancellation gesture. System Back and two-finger touch both reach the focused window's `onReturn()`, but only the latter emits right-mouse input.

Physical Enter and Space use the Confirm action, while Escape uses Cancel. Keyboard press and release are edge events; see [Engine: Input and Services](<../Global and Core Modules/Core Modules/Engine/Input and Services.md>).

Controller buttons use the logical `Engine.JoystickButton.getA()` through `getShare()` values. Confirm is A/Cross and Cancel is B/Circle. Per-controller resolution, mappings and hints are in [Engine: Input and Services](<../Global and Core Modules/Core Modules/Engine/Input and Services.md>).

Default movement and menu directions use only left-stick `X`/`Y` and D-pad `PovX`/`PovY`. Horizontal wins when `|X| >= |Y|`, and vertical wins otherwise. Exact 45-degree input is horizontal. This same sector rule drives triggered navigation and held movement, and right-stick and trigger axes cannot steal the direction.

`WindowSelectable` owns the default Cancel key and the right-button close behaviour. Derived windows return unhandled input to it, so each edge produces one Cancel sound and one close transition.

## Focus and navigation

Keyboard and controller navigation needs an explicit focus order. A disabled or hidden control must not remain the active focus target.

## Configuration controls

Config opens on Graphics with its first row focused. Q/E, LB/RB, mouse hover and touch taps switch tabs and play CursorSE. Each tab keeps its cursor and scroll during that open session. Reopening resets all tabs, while a locale refresh preserves them.

Switching tabs silently collapses an open DropBox. Cancel and the return button also collapse it first, then close Config on the next input. [Game Settings Reference](<Game Settings Reference.md>) defines the settings.

On mobile, a Slider tap sets its value on release. After the drag threshold, horizontal movement belongs to the Slider, while vertical or tied movement belongs to the Config list. Gesture ownership remains fixed until release, and the list scrolls only when its content exceeds the visible area.

An expanded Config DropBox overlays the settings rows and owns input until it collapses or Config closes. Either restores parent-list input. A height-limited popup scrolls internally, and mobile touch uses preview-then-confirm. See [Engine: Interactive and Declarative UI Controls](<../Global and Core Modules/Core Modules/Engine/Interactive and Declarative UI Controls.md>).

Save/Load uses one shared slot list. Switching the Load/Save tabs preserves its cursor, scroll and detail. Reopening defaults to Load and the newest existing save, or slot zero when none exists; map hotkey S opens Save instead. Cancel closes it. Title load-only mode omits the tabs and centres the remaining panes.

Exact UI and input APIs are in the API reference, starting with [GlobalFunctions.Container](<../Node Functions/Container.md>). Game-localised labels follow [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md>). Editor language switching is not part of this runtime section.
