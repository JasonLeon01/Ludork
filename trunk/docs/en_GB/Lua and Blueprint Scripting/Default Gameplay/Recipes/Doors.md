# Doors

Create map doors that open with an inventory key or when a named game variable reaches a threshold. Both workflows build on `Source.MapActors.DoorBase.DoorBase` with a door sprite sheet and matching Item or Game Variable identifiers.

`DoorBase` inherits `tickable = false` and activates ticks only while opening or reversing. Once the opening animation finishes, `DoorBase` records its map-placement tag as destroyed (if non-empty) so rebuilt maps and loaded saves keep the door open.

## Key door

1. Create a shared base door Blueprint derived from `Source.MapActors.DoorBase.DoorBase` and configure its texture, source rectangle, collision and opening defaults.
2. Derive a colour-specific door Blueprint — for example, Default Gameplay's `BP_Door_Y.json` inherits `BP_Door_General` and sets `needKeyID = "KEY_Y"`.
3. In `onCollision`, return early if the door is already opening and check `MeetPlayer`.
4. Read `needKeyID`, call `GetItemCount` and verify the count is at least `1`.
5. On success, invoke the parent collision flow via `SUPER` and remove one key. Leave the failure branch unconnected or attach custom feedback.

The configured `needKeyID` must match both the Item General Data member and its pickup Blueprint (such as `KEY_Y` and `BP_Item_Key_Y.json`).

## Conditional door

Attach `Mixins/Doors/ConditionDoor.lua` to a `DoorBase` Blueprint to poll `openConditionName` against `openConditionVal`:

- Leaving `openConditionName` empty disables polling; naming an uninitialised variable treats its starting value as `0`. Reaching `openConditionVal` calls `openDoor`.
- `openConditionName` uses a numeric `InstVar` selector listing `int` and `float` entries from the Game Variable Manager, and `openConditionVal` uses `InstVarValue` to match that variable's numeric control. Default Gameplay provides `MTExample = 0` for testing this workflow.

> **Note:** Doors do not perform scene transfers — use `Teleporter` for map-to-map travel. Renaming a key or game variable requires updating every stored door reference manually.
