# Actors, Enemies, Items and Equipment

This page documents Default Gameplay's runtime rules for map Actors, visibility, movement and hazard pathfinding, enemy combat, collectible items and equipment. Projects can extend or replace these default combat and inventory rules.

## Actors and characters

`Actor` provides map ownership, grid/pixel position, lifecycle hooks and collision behaviour; character subclasses add movement and facing direction. Place public class defaults directly on the Lua class table and reserve `init` for constructor parameters and private runtime state.

- **Materials**: Reference `Data/Materials/...json` via `material` and store per-instance overrides in `materialParameters` (allowing multiple Actors to share one material asset with distinct parameters). Assign `material` before setting parameters; runtime overrides require explicit state recording to persist across saves. Materials do not alter movement speed. See [Material resources](<../Global and Core Modules/Core Modules/Engine/Maps and Materials.md#material-resources>).
- **Visibility (`Actor.visible`)**: Defaults to `true`. Direct assignment updates only that Actor (`getVisible()` reads it); `setVisible(value, applyToChildren = true)` can propagate to descendants. An Actor hidden directly or through an ancestor does not render, block light, collide, overlap, occupy pathfinding cells or accept map-click interaction, and hidden enemies contribute neither movement hazards nor Enemy Book entries. Changing visibility updates affected caches while preserving the instance, keeping `Tick` active and leaving collision and animation settings intact.
- **Particle emitters (`emitterComp`)**: Configure an `Engine.EmitterComponent` to attach a [Common Particle](<../../Editor User Guide/Common Particles.md>) effect that tracks the Actor transform and draws immediately before or after its owner in list order. Hidden or dormant Actors freeze GPU playback while preserving state and manual pause; destroy, world eviction, map replacement and scene exit release the effect.
- **Billboards and child Actors**: Configure `billboardComp` ([`Engine.BillboardComponent`](<../Global and Core Modules/Core Modules/Engine/Gameplay Types.md#billboardcomponent>)) for overhead images or text on player approach, and `childActorComp` for nested Actors such as `Enemy` damage readouts (see [ChildActor Component](<../../Editor User Guide/Actor Components/ChildActor.md>)). See [Actor Components](<../../Editor User Guide/Actor Components/Overview.md>) for editor setup, including lights.

### Conditional appearance

[`Source.MapActors.ConditionalActor`](<../Source Classes/ConditionalActor.md>) adds `conditionVariable`, `conditionOperator` and `conditionValue` to toggle visibility automatically from a declared game variable:

- **Configuration**: Choose a declared numeric, boolean or string variable, an operator and a target value (the editor picks the input control from the variable type). An empty variable name disables conditional control. Numbers support `==`, `~=`, `>`, `>=`, `<` and `<=`; booleans and strings support `==` and `~=`. Invalid data raises an error without implicit conversion.
- **Inheritance**: `Enemy`, `Item`, `Equip`, `DoorBase`, `Teleporter` and Default Gameplay's direct map-Actor Blueprints inherit `ConditionalActor`. `Bottle` and `Gem` use Script Mixins, and `EnemyDamageText` keeps its own hint and `Tick` policy.
- **Subscription lifecycle**: `onCreate` subscribes and evaluates immediately; subsequent writes toggle visibility dynamically, and hidden Actors keep listening. `onDestroy` and world sleep unsubscribe, while world wake subscribes and re-evaluates. Always chain parent lifecycle events (in graph Blueprints, keep the [Parent event call](<../Blueprint Scripting/Execution Flow Events and Variables.md#parent-event-calls>) on the execution path).

See [SceneMap](<../Source Classes/SceneMap.md>) for map load and lifecycle order, and [ConditionalActor](<../Source Classes/ConditionalActor.md>) for subscriptions and persistence.

### Actors in a composite world

- **Unified registry**: `WorldGameMap` maintains one world-coordinate Actor registry; crossing a ready child boundary rehomes the same instance. Holes are part of the world: colliding Actors cannot enter unloaded placements, while non-colliding roots sleep and request unloaded areas asynchronously.
- **Chunk activation**: Only roots in `Active` 32×32-cell chunks simulate or render. Leaving invokes `onWorldSleep()` and returning invokes `onWorldWake(elapsedSeconds)` without replacing `onCreate` or `onDestroy`. Never-activated `Prepared` roots receive no lifecycle calls and are excluded from gameplay lookup.
- **Draw order and layers**: Tiles and Actors draw in manifest `layerOrder`, with runtime-only layers appended in first-use order and the player drawn last on its layer. Hiding a child tile layer also hides its Actors from colour and lighting surfaces.
- **Lookup and suppression**: `getActorByTag` searches `Active` Actors and resident roots that slept after activation without loading absent children; evicted Actors stay absent until reload. `SceneMap:recordDestroyedActorTag` suppresses that tag across resident and future child payloads without loading absent children (preventing lifecycle execution, lookup or reappearance after eviction), whereas `recordDestroyedActor` is record-only.

## Movement, pathfinding and animation

Scripted [movement and pathfinding](<../Node Functions/Movement.md>) and [door animations](<../Source Classes/DoorBase.md>) return `Engine.AsyncOperation` so Lua and Blueprints can await completion.

### Arrival, Dir4 pathfinding and click targets

- **Stopping and arrival**: Stopping movement (including when opening a UI) processes arrival whenever the player snaps into a new cell, so overlaps and movement-step effects are not skipped. Danger-driven click-route replanning and instant route completion both return to the last complete cell before stopping; instant completion then applies destination overlaps and the collected movement path once.
- **Dir4 and footprints**: A* and actual movement enforce the same Dir4 edge check: every newly entered leading cell must permit departure from its source tile and entry through the opposite side of its destination tile. Passability and pathfinding place the Actor footprint squarely on the queried grid cell without ongoing pixel offsets, keeping hazard line checks independent of movement direction and progress.
- **Click fallback and overlap avoidance**: Click movement uses direct A* for normal goals and 1-cell blocked targets; if the Actor's footprint blocks a direct route, it falls back to searching adjacent cells. Colliding targets execute their final bump/collision step, whereas non-colliding impassable targets stop at the adjacent cell. Pathfinding routes around non-colliding Actors with an executable `onOverlap` during transit, but explicitly clicking such an Actor (such as a Teleporter) keeps the route onto its cell and triggers `onOverlap`.
- **Composite worlds**: World pathfinding crosses loaded child boundaries and treats unloaded children as impassable without triggering implicit I/O. Holes are passable with a `nil` top material.

### Map viewport (`GameMap.MapViewRect`)

`GameMap.MapViewRect` defines the logical map canvas (default `sf.IntRect.new(192, 32, 416, 416)`), setting both the clipped drawing area and Camera viewport size. Map axes smaller than the canvas centre automatically; larger axes clamp to scrollable bounds. Map rendering, pointer conversion, world/UI positioning, animations, lighting, fog, panorama and weather share this rectangle, and pointer input outside it is ignored.

### Movement hazards (`Domain`, `Flank`, `Blockade`)

- **Spatial rules**: `Domain`, `Flank` and `Blockade` grant `Gameplay.Movement.Hazard`. Both movement and `MovementDangerGrid` query `MovementSpecials.Preview` (dispatching `Event.Movement.QueryHazard`), keeping spatial logic unified. `Domain` affects cells within Manhattan distance `<= range` (minimum `1`, so `Domain = 1` covers the four orthogonal neighbours only) and respects combat specials (such as `Magic` ignoring DEF). `Flank` requires enemies on opposite adjacent sides, and `Blockade` affects adjacent cells; positive contributions sum.
- **Hints and click routes**: Once the player owns the Enemy Book, the shared [Gameplay damage-hint setting](<Runtime Data Configuration and Saves.md#shared-gameplay-configuration>) controls enemy readouts and danger-cell totals without gameplay side effects. Click routes avoid positive-danger cells even when hints are hidden, except for an explicitly clicked destination. Clicking a `Domain` or `Blockade` enemy ignores only that target's contributions (including its side of a `Flank`); collision and overlap targets the player will not occupy ignore destination-cell danger.
- **Lazy danger grid**: Grids materialise only for map hints or click pathfinding, scanning cells inside `Event.Movement.QueryHazardBounds` coverage ([MovementSpecials](<../Source Classes/Gameplay.md#movementspecials-and-movement-danger>)) and querying only relevant enemies via `Preview`. Area shifts reuse overlapping cells (including zero-damage cells), ordinary maps reuse a stable cache and single-cell queries stay local.
- **Cache invalidation and replanning**: Chunk activation/sleep and enemy or ability edits invalidate affected danger caches. A danger revision invalidates active movement only when it affects the remaining path or an enterable enemy goal, returning to the last complete cell to replan.

## Enemies

An Enemy Blueprint references an Enemy General Data member. `GeneralDataTypes.Create` copies it into an `EnemyAttributeSet`, exposed as `Battler.attributes` alongside its `AbilitySystemComponent`. Enemy General Data has no member events; combat logic uses Gameplay Abilities and Effects.

### Attributes and specials

- **HP and progression**: `MAXHP` is an enemy's virtual durability (enemies have no mutable `HP`). Player `HP` lives in `PlayerAttributeSet`, starts at `1000` in Player General Data and is not overridden as a Blueprint attribute. Player `HP` Current clamps to `0..MAXHP` after modifiers; permanent `MAXHP` and `LEVEL` growth reacts to Base changes so temporary Infinite Effects never alter permanent progression.
- **Enemy `special` entries**: Store integer counts/ranges, presence-only `""`, the `Vampire` ratio, the `FixDmg` number/expression and the `Reborn` class path. Each entry becomes an Infinite `Special.<id>` Effect with its matching tag and ability (combat specials hook combat Gameplay Events; movement specials use `Event.Movement.QueryHazard` and `Gameplay.Movement.Hazard`).

### Combat resolution, previews and defeat settlement

- **Combat calculation**: Every enemy grants `Ability.Combat.MotaBattle`. Its pure calculation produces a damage breakdown and numeric `Enums.BattleResultCode` (`WIN`, `CANNOT_DAMAGE`, `LETHAL_COUNTER_DAMAGE`). Previews reuse it without mutation, and collision validates damage, Game Over, rewards and State Effect Specs before calling `CommitResult(result)`.
- **Attack timing and Game Over**: Counter damage displays at the earliest `dmg` time tag of the enemy's attack animation via a Scene timer (or immediately if absent). HP commits immediately, and settlement waits for both attack animations to finish. Lethal movement-hazard (`Domain`/`Flank`/`Blockade`) damage similarly applies immediately and enters Game Over after the longest concurrently started attack animation completes.
- **Defeat settlement order**: After a win and both attack animations finish, the Enemy Actor Blueprint `onDefeat` event runs (Enemy General Data has no `onDefeat` graph). On completion, fixed settlement runs in order: apply `afterBattleVarChanges` -> record and destroy the Enemy -> spawn `drops` or a `Reborn` replacement -> grant rewards and States. The [defeat shatter effect](<Resources Audio Video and Effects.md#shaders-fog-and-transitions>) is visual only; destroyed enemies stop blocking movement and light immediately.
- **Drops and `Reborn`**: `drops` maps Item Blueprint paths to `sf.Vector2i` cell offsets, spawning on the enemy's layer, persisting as added Actors and activating their General Data `onDrop` graph via `GeneralDataGraphAbility` (duplicate placement tags are skipped; invalid or non-Item paths raise an error). `Reborn` uses its non-empty Blueprint class path to spawn a replacement on the same cell and layer with a fresh map-placement tag, running normal `onCreate` and recording it as an added Actor so map rebuilds and saves keep the original destroyed and restore the replacement once (invalid paths fail).
- **`afterBattleVarChanges`**: Stores `variableName = { operator, operand }` for declared `int`/`float` variables using `=`, `+`, `-`, `*`, `/`, `//`, `%` or `**` (serialised as a JSON object of two-element arrays). Operations validate before combat damage and re-evaluate against live variables after `onDefeat` through fixed `Engine.Eval` expressions with fresh locals — stored operators never execute as raw source.
- **Damage-preview formulas**:
  - Without healing, enemy max HP `H` and positive incoming hit damage `D` yield `max(ceil(H / D) - 1, 0)` counterattacks.
  - With `Vampire = R`, each non-final counterattack heals `floor(counterDamage * R)`. If a non-one-shot hit satisfies `D <= healing`, the player cannot win; otherwise counterattacks equal `ceil((H - D) / (D - healing))`.
  - `First` adds one enemy strike before the player's first hit (it cannot heal an enemy at full HP).
  - `FixDmg` uses a numeric literal directly or evaluates an expression after substituting live `{m<attribute>}` and `{e<attribute>}` values, flooring the finite non-negative result and adding it after round and first-strike damage.

## Items

- **Member graphs (`onUse` and `onDrop`)**: Run as explicitly activated `GeneralDataGraphAbility` graphs rather than Actor events. `onUse` targets the Player (place Player State nodes here; `EnemyBook` opens the monster handbook via [`GlobalFunctions.Scene.ShowEnemyBook`](<../Node Functions/Scene.md#showenemybook>)), while `onDrop` targets the spawned Item Actor. Because the graph owner is `GameplayEventData` (including inside Common Functions), connect `GameplayEventData.target` to the visible `self` pin when invoking Player or Item methods.
- **Stacked item collection**: Collision callbacks on the highest occupied tile layer execute from visually topmost to bottommost Actor (later entries on a layer draw and trigger first). A blocked step into stacked collectible items picks up the topmost item and halts that collision pass; subsequent steps (or holding a direction across fixed updates) collect remaining items one by one.
- **Reusable teleport item nodes**: `GlobalFunctions.Mota.CenterSymmetricTeleport`, `GoUpstairsSamePos` and `GoDownstairsSamePos` back `CenterSymmetricTeleport`, `UpstairsTransportation` and `DownstairsTransportation`. Adjacent-floor nodes follow the current region's map order and validate the persisted target map at the player's zero-based cell before transferring; centre-symmetric teleport targets `(width - 1 - x, height - 1 - y)` on the current map. Each node exposes `Success` and `Failed` outputs; failure shows localised `FLY_FAIL` as a common tip without consuming the item — wire `GlobalFunctions.Player.RemoveItem` to `Success` only.

## Equipment

- **Stat bonuses and graphs**: Equipment `attrPlus` applies as a slot-sourced Infinite Effect before the `onEquip` graph runs and removes before `onUnequip` runs. Slot names must match the active Class definition.
- **States**: States are Infinite Effects carrying `State.<id>`, stack counts, attribute modifiers and abilities. `Weak` subtracts `1` ATK/DEF per stack (clamped at `0`), shown on the HUD as Current plus Weak stacks (e.g. `8 (-2)` for base `10` with two stacks). `Poisoned` adds damage via `Event.Combat.ResolveIncomingDamage`. State graphs subscribe to `Event.Movement.Step` (`onWalk`) or `Event.State.Trigger.<id>` (explicit hook).

## Runtime references

Treat General Data identifiers as stable, language-independent keys: class, animation and equipment-slot keys are stored and compared verbatim. Display names and descriptions may use brace-wrapped localisation keys — resolve them with `LOC` only at display time and refresh on language changes (see [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md>)). Renaming any identifier requires updating all stored references.

For exact database schemas and member lists, start with [Class](<../../Editor User Guide/General Data Reference/Class.md>). For practical authoring walkthroughs, start with [Consumables](<Recipes/Consumables.md>).
