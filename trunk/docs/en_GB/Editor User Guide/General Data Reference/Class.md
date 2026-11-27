# Class

Defines player class presets, including their display metadata, available equipment slot names and initially equipped items.

**Source:** `Data/General/Class.json` · **Member events:** None

## Schema

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | `string` | `""` | Display name of this class. |
| `desc` | `string` | `""` | Display description of this class. |
| `slot` | `Dict[string, string]` | `{}` | Equipment slot names mapped to the equipment ID initially equipped in each slot (an empty string leaves the slot unequipped). |
