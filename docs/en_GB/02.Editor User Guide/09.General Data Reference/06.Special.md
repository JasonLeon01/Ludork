# Special

Source: `Data/General/Special.json`

| Property | Value |
|---|---|
| Linked runtime type | — |
| Member events | — |

## Schema

| Field | Type | Default | Description | Reference |
|---|---|---|---|---|
| `name` | `string` | `""` | The display name of this special. | `—` |
| `desc` | `string` | `""` | The display description of this special. | `—` |
| `icon` | `file` | `""` | Full logical icon path. The selector root is `/Game/Assets/Icons`. | `—` |

## Editing notes

Define fields before populating many members. A reference controls the selector, but the stored value keeps the declared JSON shape. When the type lists events, each member may store a graph under `_graph`. An absent start node means that the optional event has no implementation.

Special records define presentation only. Parameters remain in `Enemy.special`. Presence-only specials use `""`. A special's description may contain one `{value}` placeholder. The Enemy Book localises `FixDmg` expression tokens before inserting the value into the description.
