# Enemy

定义敌人的战斗属性、击败奖励、物品掉落、特殊能力参数与攻击动画。

**来源**：`Data/General/Enemy.json` · **成员事件**：无（击败行为由玩法能力处理）

## Schema

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `name` | `string` | `"NEW ENEMY"` | 该敌人的显示名称。 |
| `desc` | `string` | `"NEW ENEMY DESC"` | 该敌人的显示说明。 |
| `MAXHP` | `int` | `0` | 最大生命值。 |
| `ATK` | `int` | `0` | 攻击力。 |
| `DEF` | `int` | `0` | 防御力。 |
| `EXP` | `int` | `0` | 击败后获得的经验值。 |
| `GOLD` | `int` | `0` | 击败后获得的金币。 |
| `drops` | `Dict[string, sf.Vector2i]` | `{}` | 物品蓝图类路径到地图格偏移量（以敌人被击败格为原点）的映射。 |
| `special` | `{ dict = "any", key = { enum = "Enums.GeneralData.Special", valueType = "string" } }` | `{}` | `Special` 成员 ID 到带类型参数的映射。 |
| `ANIMATION_KEY` | `{ enum = "Enums.Animation", valueType = "string" }` | `""` | 攻击动画键。 |

## 编辑说明

- **特殊能力参数（`special`）**：
  - **存在即生效型**（`Compete`、`Hard`、`Magic`、`First`、`Flank`、`Blockade`）：值填 `""`，只要键存在即启用该能力。
  - **`Vampire`**：有限非负数。
  - **`Reborn`**：非空蓝图类路径。
  - **`FixDmg`**：有限非负数或非空表达式字符串。仅字符串形式会在调用 `Engine.Eval` 前代入 `{m<attribute>}`（玩家属性）与 `{e<attribute>}`（敌人属性）占位符；其余特殊能力取值不做表达式求值。
- **敌人掉落（`drops`）**：每个键必须是指向 `Source.MapActors.Item` 派生类的蓝图路径，其 `sf.Vector2i` 偏移会叠加到敌人被击败的地图坐标上。生成 Actor 的标签由蓝图路径与绝对目标格推导得出；若同名标签已存在，则跳过该次掉落并记录警告，不生成带后缀的重复对象。
