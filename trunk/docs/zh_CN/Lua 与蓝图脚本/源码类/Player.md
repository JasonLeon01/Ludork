# Source.MapActors.Player

`Player` 是可操控的 Character 与战斗 Actor，声明了 Character 默认属性，并提供面向蓝图与 Lua 的玩家命名、背包、装备、属性与持久化 API。

**元数据来源**：`Scripts/Source/MapActors/Player_meta.lua`

**继承**：`Engine.Character`、`Source.Battler`

编辑器会显示只读的生成 `PlayerAttributeSet` 预览，该预览不会序列化到蓝图 JSON 中。

## 属性

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `ID` | `string` | `"FILL_IT_BY_YOURSELF"` | `Enums.GeneralData.Player` 枚举选择器。 |
| `tickable` | `bool` | `true` | — |
| `collisionEnabled` | `bool` | `true` | — |
| `animatable` | `bool` | `true` | — |
| `speed` | `float` | `96` | — |

`attributes` 为生成的 `PlayerAttributeSet`。背包、装备、移动控制与 Ability System 均为运行时状态。

## 蓝图函数与事件

实例方法省略隐式接收者 `self`（`Source.MapActors.Player`）；在蓝图中，该参数作为首个目标引脚暴露。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getDisplayName` | — | `name: string` | 纯函数（Pure）。返回自定义名字原文，或通用数据中的本地化默认名。 |
| `setName` | `name: string` | `success: bool` | 执行分支：`Success`、`Invalid`。校验并更新玩家自定义名字。 |
| `onFixedTick` | `fixedDelta: float` | — | 事件（Event）。默认执行出口。 |
| `addItem` | `itemID: string`, `count: int = 1` | — | `Enums.GeneralData.Item` 选择器。向背包添加物品。 |
| `removeItem` | `itemID: string`, `count: int = 1` | `bool` | 执行分支：`Success`、`Failed`。`Enums.GeneralData.Item` 选择器。 |
| `getItemCount` | `itemID: string` | `count: int` | 纯函数（Pure）。`Enums.GeneralData.Item` 选择器。 |
| `hasItem` | `itemID: string` | `value: bool` | 纯函数（Pure）。`Enums.GeneralData.Item` 选择器。 |
| `addEquip` | `equipID: string`, `count: int = 1` | — | `Enums.GeneralData.Equip` 选择器。向背包添加装备。 |
| `removeEquip` | `equipID: string`, `count: int = 1` | `bool` | 执行分支：`Success`、`Failed`。`Enums.GeneralData.Equip` 选择器。 |
| `equip` | `equipID: string` | — | `Enums.GeneralData.Equip` 选择器。穿戴指定装备并更新其无限持续效果。 |
| `unequip` | `slotID: string` | — | 卸下指定槽位的装备。默认执行出口。 |
| `getEquipCount` | `equipID: string` | `count: int` | 纯函数（Pure）。`Enums.GeneralData.Equip` 选择器。 |
| `hasEquip` | `equipID: string` | `value: bool` | 纯函数（Pure）。`Enums.GeneralData.Equip` 选择器。 |
| `getEquipInfo` | `slotID: string` | `value: string` | 纯函数（Pure）。返回 `slotID` 槽位当前穿戴的装备 ID。 |
| `getForbiddenMoving` | — | `value: bool` | 纯函数（Pure）。返回当前是否禁止玩家移动。 |
| `setForbiddenMoving` | `value: bool` | — | 启用或解除玩家移动限制。默认执行出口。 |

## Lua 运行时 API

- **`Player.MAX_NAME_GRAPHEMES`**（`int`，`32`）— 以字素簇计数的玩家名字长度上限。

实例方法省略隐式接收者 `self`（`Source.MapActors.Player`）；静态方法直接通过 `Player` 调用。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `ValidateName` | `name: string` | `string`, `Enums.PlayerNameValidationError?` | 静态方法。返回去除首尾空白后的候选名字与校验错误（合法时为 `nil`）。 |
| `init` | `texture?`, `tag?` | — | 构造 Attribute Set、Ability System 以及空的背包与装备状态。 |
| `InitPlayer` | `playerPath: string`, `applyInitialEquipment?: bool = true` | `Source.MapActors.Player` | 静态方法。按蓝图类路径构造 Player，并可选应用职业（Class）初始装备。 |
| `FromDict` | `data: table` | `Source.MapActors.Player` | 静态方法。恢复基础值、背包能力、装备效果与 State 效果，不应用职业初始装备，也不执行成员图。 |
| `asDict` | — | `SaveData` | 序列化稳定状态，不包含运行时效果句柄。 |
| `activateItem` | `itemID: string` | `GlobalCore.GameplayAbilityResult` | 激活 `GeneralData.Item.<id>.onUse`。 |
| `consumeMovementSpecialPath` | — | `sf.Vector2i[]` | 取出并清空已到达的地图格路径。 |
| `getClassPath` | — | `string` | 返回可用于重建的蓝图类路径。 |
| `setClassPath` | `path: string` | — | 设置可用于重建的蓝图类路径。 |
| `getItems` | — | `table<string, int>` | 返回独立的物品数量快照；修改玩家背包请使用背包命令。 |
| `getEquips` | — | `table<string, int>` | 返回独立的装备数量快照；修改玩家装备请使用装备命令。 |
| `restoreFromData` | `data: table` | — | 将存档状态恢复到同一蓝图类的 Player 实例中，不重放成员图。 |
| `MeetPlayer` | `actors: Engine.Actor[]`, `player: Source.MapActors.Player` | `Source.MapActors.Player?` | 静态方法。仅当 Actor 数组中包含指定玩家时返回该 Player 实例。 |

在运行时穿戴或卸下装备时，系统先更新对应的无限持续效果，再执行相应成员图。玩家移动完成后会派发 `Event.Movement.Step`。

HP 当前值限制在 `0..MAXHP` 范围内。永久的 MAXHP 与 LEVEL 成长通过修改 Base 增量实现，效果与约束（Constraint）的变化不会改写成长基础值。

`setName` 会去除首尾 Unicode 空白字符，仅接受 1 至 `Player.MAX_NAME_GRAPHEMES` 个字素簇。`Enums.PlayerNameValidationError.EMPTY` 为 `"empty"`，`TOO_LONG` 为 `"tooLong"`；输入不合法时名字保持不变。`getDisplayName` 返回自定义名字原文，或在未自定义时返回通用数据的本地化默认名。
