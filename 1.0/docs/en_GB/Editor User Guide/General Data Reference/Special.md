# Special

Source: `Data/General/Special.json`

| Property | Value |
|---|---|
| Linked runtime type | — |
| Member events | — |

## Schema

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | `string` | `""` | The display name of this special. |
| `desc` | `string` | `""` | The display description of this special. |
| `icon` | `file` | `""` | Full logical icon path. The selector root is `/Game/Assets/Icons`. |

## Editing notes

Special records define presentation only. Parameters remain in `Enemy.special`. Presence-only specials use `""`. A special's description may contain one `{value}` placeholder. The Enemy Book localises `FixDmg` expression tokens before inserting the value into the description.
