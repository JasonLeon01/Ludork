# Enemy

Defines enemy combat statistics, defeat rewards, item drops, special ability parameters and attack animation.

**Source:** `Data/General/Enemy.json` · **Member events:** None (Gameplay Abilities handle defeat behaviour)

## Schema

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | `string` | `"NEW ENEMY"` | Display name of this enemy. |
| `desc` | `string` | `"NEW ENEMY DESC"` | Display description of this enemy. |
| `MAXHP` | `int` | `0` | Maximum HP. |
| `ATK` | `int` | `0` | Attack value. |
| `DEF` | `int` | `0` | Defence value. |
| `EXP` | `int` | `0` | Experience granted on defeat. |
| `GOLD` | `int` | `0` | Gold granted on defeat. |
| `drops` | `Dict[string, sf.Vector2i]` | `{}` | Item Blueprint class paths mapped to map-cell offsets from the defeated enemy. |
| `special` | `{ dict = "any", key = { enum = "Enums.GeneralData.Special", valueType = "string" } }` | `{}` | `Special` member IDs mapped to their typed parameters. |
| `ANIMATION_KEY` | `{ enum = "Enums.Animation", valueType = "string" }` | `""` | Attack animation key. |

## Editing notes

- **Special trait parameters (`special`):**
  - **Presence-only traits** (`Compete`, `Hard`, `Magic`, `First`, `Flank`, `Blockade`): store `""` — the key's presence enables the trait.
  - **`Vampire`:** Finite non-negative number.
  - **`Reborn`:** Non-empty Blueprint class path.
  - **`FixDmg`:** Either a finite non-negative number or a non-empty expression string. Only the string form substitutes `{m<attribute>}` (player) and `{e<attribute>}` (enemy) placeholders before `Engine.Eval`; no other special value is evaluated.
- **Enemy drops (`drops`):** Every key must be a Blueprint class path whose Actor derives from `Source.MapActors.Item`. Its `sf.Vector2i` offset adds to the defeated enemy's map coordinates. The spawned Actor's tag derives from the Blueprint path and absolute target cell; if an identical tag already exists, that drop is skipped with a warning rather than spawning a suffixed duplicate.
