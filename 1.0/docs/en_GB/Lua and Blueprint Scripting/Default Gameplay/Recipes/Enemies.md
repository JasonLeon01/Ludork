# Enemies

Create an enemy whose physical Blueprint and General Data combat definition remain independently reusable.

## Prerequisites

- A valid Enemy General Data file and an Actor-compatible enemy parent.
- Existing Item Blueprint paths, special-effect identifiers and animation identifiers referenced by the new record.

## Steps

1. Add an Enemy member with combat values, rewards, drops, special effects and animation key.
2. Create or derive an Enemy Blueprint and point it at the data member.
3. Configure movement, collision and presentation defaults.
4. Put custom defeat choreography in the Enemy Actor `onDefeat` hook or a subclass-granted Gameplay Ability. Enemy General Data has no events, so fixed settlement waits for `onDefeat`.

Shared combat algorithms belong in Gameplay Abilities, Gameplay Effects or Lua modules.

## Game walkthrough

`BP_Enemy_Slime.json` derives from `Source.MapActors.Enemy` and sets `ID = "Slime"`. That ID resolves the `Slime` member in `Data/General/Enemy.json`. The Blueprint owns physical and presentation defaults such as the texture, source rectangle and material speed. The General Data member owns `MAXHP`, `ATK`, `DEF`, `EXP`, `GOLD`, `drops`, `special` and `ANIMATION_KEY`.

Configure the optional [enemy-defeat shatter effect](<../Resources Audio Video and Effects.md#shaders-fog-and-transitions>) through [System Config → Gameplay](<../../../Editor User Guide/Project Settings and Asset Layout.md#system-config>).

Set `drops` keys to Item Blueprint paths, with `sf.Vector2i` cell offsets. See [Enemies](<../Actors Enemies Items and Equipment.md#enemies>) for drop activation, persistence and defeat settlement.

`GhostSoldier` demonstrates `special.Reborn = "Data.Blueprints.Enemies.BP_Enemy_Skeleton_01"`. It replaces the defeated enemy on the same cell and layer; settlement assigns a fresh map-placement tag so the replacement survives map rebuilds.

## Limitations

Unknown IDs and missing referenced records are errors. The runtime does not synthesise replacement combat definitions.

## Related pages

- [Enemy](<../../../Editor User Guide/General Data Reference/Enemy.md>)
- [Source.MapActors.Enemy](<../../Source Classes/Enemy.md>)
