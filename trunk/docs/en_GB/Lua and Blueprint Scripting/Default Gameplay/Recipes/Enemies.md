# Enemies

Create an enemy whose physical Actor Blueprint and General Data combat statistics stay decoupled and independently reusable. Ensure the referenced Item Blueprint paths, special-effect IDs and animation keys exist before linking them.

## Create an enemy Blueprint and data record

1. Add a member to Enemy General Data with its combat stats, rewards, drops, special effects and animation key.
2. Create or derive an Enemy Blueprint from `Source.MapActors.Enemy` (or a compatible subclass) and set `ID` to that member key.
3. Configure movement, collision and presentation defaults on the Blueprint.
4. Implement custom defeat sequences in the Enemy Actor's `onDefeat` event or a subclass-granted Gameplay Ability. Enemy General Data has no member events, and fixed settlement waits for `onDefeat` to finish.

Place reusable combat algorithms in Gameplay Abilities, Gameplay Effects or shared Lua modules.

## Default template examples

- **`BP_Enemy_Slime.json`**: Derives from `Source.MapActors.Enemy` and sets `ID = "Slime"`, resolving `Slime` in `Data/General/Enemy.json`. The Blueprint owns physical and visual defaults (texture, source rectangle, material speed), while General Data owns `MAXHP`, `ATK`, `DEF`, `EXP`, `GOLD`, `drops`, `special` and `ANIMATION_KEY`.
- **Drops and defeat shatter**: Map `drops` keys to Item Blueprint paths with `sf.Vector2i` cell offsets (see [Enemies](<../Actors Enemies Items and Equipment.md#enemies>) for drop activation, persistence and settlement order). Toggle the project-wide [enemy-defeat shatter effect](<../Resources Audio Video and Effects.md#shaders-fog-and-transitions>) in [System Config → Gameplay](<../../../Editor User Guide/Project Settings and Asset Layout.md#system-config>).
- **Reborn replacement (`GhostSoldier`)**: Sets `special.Reborn = "Data.Blueprints.Enemies.BP_Enemy_Skeleton_01"`. On defeat, settlement spawns the replacement on the same cell and layer with a fresh map-placement tag so it persists across map rebuilds and saves.

> **Important:** Unknown enemy IDs and missing referenced records raise explicit runtime errors; the engine never synthesises fallback combat definitions.
