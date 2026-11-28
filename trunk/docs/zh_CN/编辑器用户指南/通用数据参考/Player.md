# Player

定义可操控角色预设，关联指定 `Class` 成员并配置初始战斗属性、成长数值与攻击动画。

**来源**：`Data/General/Player.json` · **成员事件**：无

## Schema

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `name` | `string` | `""` | 该玩家预设的显示名称。 |
| `desc` | `string` | `""` | 该玩家预设的显示说明。 |
| `CLASS` | `{ enum = "Enums.GeneralData.Class", valueType = "string" }` | `""` | 定义装备槽位与初始装备的 `Class` 成员 ID。 |
| `MAXHP` | `int` | `1000` | 初始最大生命值。 |
| `HP` | `int` | `1000` | 初始当前生命值。 |
| `ATK` | `int` | `10` | 初始攻击力。 |
| `DEF` | `int` | `10` | 初始防御力。 |
| `EXP` | `int` | `0` | 初始经验值。 |
| `GOLD` | `int` | `0` | 初始金币。 |
| `LEVEL` | `int` | `1` | 初始等级。 |
| `ANIMATION_KEY` | `{ enum = "Enums.Animation", valueType = "string" }` | `""` | 攻击动画键。 |

## 编辑说明

在 Game 项目中，`Bravor` 成员配置了 `MAXHP = 2000` 与 `HP = 1000`。运行时会将所选成员复制到 `PlayerAttributeSet` 中；`Data/Blueprints/Players/BP_Bravor.json` 蓝图仅保存成员 `ID`。
