# Equip

定义可穿戴装备、所占用的 `Class` 槽位、装备期间的被动属性加成，以及可选的穿戴/卸下能力图。

**来源**：`Data/General/Equip.json` · **成员事件**：`onEquip`、`onUnequip`

## Schema

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `name` | `string` | `""` | 该装备的显示名称。 |
| `desc` | `string` | `""` | 该装备的显示说明。 |
| `icon` | `file` | `""` | 规范逻辑图标路径（选择器根目录：`/Game/Assets/Icons`）。 |
| `slot` | `string` | `""` | 该装备占用的 `Class` 槽位键。 |
| `attrPlus` | `Dict[string, int]` | `{}` | 属性名到整数修正值的映射，穿戴期间生效。 |
