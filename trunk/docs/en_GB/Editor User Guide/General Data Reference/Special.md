# Special

Defines presentation metadata (display name, description template and icon) for enemy special traits whose runtime parameters are configured in `Enemy.special`.

**Source:** `Data/General/Special.json` · **Member events:** None

## Schema

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | `string` | `""` | Display name of this special trait. |
| `desc` | `string` | `""` | Display description (may include one `{value}` placeholder). |
| `icon` | `file` | `""` | Canonical logical icon path (picker root: `/Game/Assets/Icons`). |

## Editing notes

`Special` records supply UI presentation only; trait parameters live in `Enemy.special` (presence-only traits store `""`). When displaying `FixDmg`, the Enemy Book localises expression tokens before substituting `{value}` into `desc`.
