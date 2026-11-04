# Special

来源：`Data/General/Special.json`

| 属性 | 值 |
|---|---|
| 关联运行时类型 | — |
| 成员事件 | — |

## Schema

| 字段 | 类型 | 默认值 | 描述 |
|---|---|---|---|
| `name` | `string` | `""` | 该 special 的显示名称。 |
| `desc` | `string` | `""` | 该 special 的显示说明。 |
| `icon` | `file` | `""` | 完整的图标逻辑路径。选择器根路径为 `/Game/Assets/Icons`。 |

## 编辑说明

Special 记录只定义展示内容，参数仍保存在 `Enemy.special` 中。仅靠存在与否生效的 special 取值为 `""`。special 的说明可以包含一个 `{value}` 占位符。Enemy Book 会先本地化 `FixDmg` 表达式的 token，再把值填入说明。
