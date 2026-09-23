# Enemy

Source: `Data/General/Enemy.json`

| Property | Value |
|---|---|
| Member events | — |

## Schema

| Field | Type | Default | Description | Reference |
|---|---|---|---|---|
| `name` | `string` | `"NEW ENEMY"` | The display name of this enemy. | `—` |
| `desc` | `string` | `"NEW ENEMY DESC"` | The display description of this enemy. | `—` |
| `MAXHP` | `int` | `0` | The enemy's maximum HP. | `—` |
| `ATK` | `int` | `0` | The enemy's attack value. | `—` |
| `DEF` | `int` | `0` | The enemy's defence value. | `—` |
| `EXP` | `int` | `0` | The experience granted when the enemy is defeated. | `—` |
| `GOLD` | `int` | `0` | The gold granted when the enemy is defeated. | `—` |
| `drops` | `Dict[string, sf.Vector2i]` | `{}` | Item Blueprint class paths mapped to map-cell offsets from the defeated enemy. | `—` |
| `special` | `Dict[string, any]` | `{}` | Special member IDs mapped to their typed parameters. | `general:Special` |
| `ANIMATION_KEY` | `string` | `""` | The animation played when the enemy attacks. | `animation` |

## Editing notes

The Enemy schema declares no member events. Gameplay abilities handle defeat behaviour.

In `special`, the presence of the key alone enables `Compete`, `Hard`, `Magic`, `First`, `Flank` and `Blockade`, and each of them stores `""`. `Vampire` takes a finite non-negative number, and `Reborn` takes a non-empty Blueprint class path. `FixDmg` accepts either a finite non-negative number or a non-empty expression string. Only the string form substitutes the current player and enemy values into `{m<attribute>}` and `{e<attribute>}` before `Engine.Eval`. No other special value is evaluated.

Each `drops` key must be a Blueprint class path whose generated Actor derives from `Source.MapActors.Item`. The matching `sf.Vector2i` value is added to the defeated enemy's map position. The spawned Actor tag is derived from the Blueprint path and the absolute target coordinates. When an identical tag already exists, that drop is skipped and a warning is written instead of a suffixed duplicate.
