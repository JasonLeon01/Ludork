# Game 项目 Script Mixin

默认玩法包含以下 Script Mixin 示例。每个示例均依赖表中列出的父类能力、配套 `_meta.lua` 文件以及相应的项目数据或配置。

| 脚本 | 父类能力 | 行为 |
|---|---|---|
| `Consumables/Bottle.lua` | 可碰撞 Actor | 检测 Player，恢复 `HP`，播放已配置音效并销毁拾取物。 |
| `Consumables/Gem.lua` | 可碰撞 Actor | 向 Player 的直接属性或匹配的组件字段累加数值。 |
| `Doors/Door.lua` | `DoorBase` | 碰撞时开门并防止重复开启。 |
| `Doors/KeyDoor.lua` | Script Mixin 门蓝图 | 先校验并消耗已配置的通用数据物品，再调用父类碰撞方法。 |
| `Doors/ConditionDoor.lua` | `DoorBase` | 仅在配置了游戏变量时启用轮询，变量达到阈值后自动开门。 |

完整的端到端配置示例见[消耗品](<../默认玩法/常见用法/消耗品.md>)与[门](<../默认玩法/常见用法/门.md>)。
