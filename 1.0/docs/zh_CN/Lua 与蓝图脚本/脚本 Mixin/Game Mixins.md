# Game Script Mixins

Game 项目提供以下 Mixin 示例。各脚本需要表中列出的父类功能，以及对应的元数据和项目配置。

| 脚本 | 父类能力 | 行为 |
|---|---|---|
| `Consumables/Bottle.lua` | 可碰撞 Actor | 检测 Player，恢复 HP，播放已配置的声音，然后销毁拾取物。 |
| `Consumables/Gem.lua` | 可碰撞 Actor | 向 Player 的直接字段或匹配的组件字段累加数值。 |
| `Doors/Door.lua` | `DoorBase` | 碰撞时开门，并防止重复开启。 |
| `Doors/KeyDoor.lua` | Script Mixin 门蓝图 | 先要求并消耗已配置的通用数据物品，再继续执行父类碰撞方法。 |
| `Doors/ConditionDoor.lua` | `DoorBase` | 只为已配置的游戏变量启用轮询，达到阈值时开门。 |

## 相关页面

- [Script Mixin 编写工作流](<编写工作流.md>)
- [消耗品](<../Game 玩法/实用配方/消耗品.md>)
- [门](<../Game 玩法/实用配方/门.md>)
