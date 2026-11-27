# Special

定义敌人特殊能力的界面展示元数据（名称、说明模板与图标），对应能力的运行时参数配置在 `Enemy.special` 中。

**来源**：`Data/General/Special.json` · **成员事件**：无

## Schema

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `name` | `string` | `""` | 该特殊能力的显示名称。 |
| `desc` | `string` | `""` | 显示说明（可包含一个 `{value}` 占位符）。 |
| `icon` | `file` | `""` | 规范逻辑图标路径（选择器根目录：`/Game/Assets/Icons`）。 |

## 编辑说明

`Special` 记录仅用于 UI 展示，实际能力参数保存在 `Enemy.special` 中（存在即生效型能力填 `""`）。展示 `FixDmg` 时，怪物手册会先本地化表达式中的标记，再将结果代入 `desc` 中的 `{value}`。
