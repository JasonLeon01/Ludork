# Equip

Defines equippable items, the `Class` slot each item occupies, its passive attribute bonuses and optional equip/unequip ability graphs.

**Source:** `Data/General/Equip.json` · **Member events:** `onEquip`, `onUnequip`

## Schema

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | `string` | `""` | Display name of this equipment. |
| `desc` | `string` | `""` | Display description of this equipment. |
| `icon` | `file` | `""` | Canonical logical icon path (picker root: `/Game/Assets/Icons`). |
| `slot` | `string` | `""` | `Class` slot key occupied by this equipment. |
| `attrPlus` | `Dict[string, int]` | `{}` | Attribute names mapped to integer modifiers applied while equipped. |
