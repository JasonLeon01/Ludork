# Player

Defines playable character presets, referencing a `Class` member and setting starting combat attributes, progression stats and attack animation.

**Source:** `Data/General/Player.json` · **Member events:** None

## Schema

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | `string` | `""` | Display name of this player preset. |
| `desc` | `string` | `""` | Display description of this player preset. |
| `CLASS` | `{ enum = "Enums.GeneralData.Class", valueType = "string" }` | `""` | `Class` member ID defining equipment slots and initial equipment. |
| `MAXHP` | `int` | `1000` | Starting maximum HP. |
| `HP` | `int` | `1000` | Starting current HP. |
| `ATK` | `int` | `10` | Starting attack value. |
| `DEF` | `int` | `10` | Starting defence value. |
| `EXP` | `int` | `0` | Starting experience. |
| `GOLD` | `int` | `0` | Starting gold. |
| `LEVEL` | `int` | `1` | Starting level. |
| `ANIMATION_KEY` | `{ enum = "Enums.Animation", valueType = "string" }` | `""` | Attack animation key. |

## Editing notes

In the Game project, the `Bravor` member sets `MAXHP = 2000` and `HP = 1000`. At runtime, the selected member is copied into `PlayerAttributeSet`; the `Data/Blueprints/Players/BP_Bravor.json` Blueprint stores only the member `ID`.
