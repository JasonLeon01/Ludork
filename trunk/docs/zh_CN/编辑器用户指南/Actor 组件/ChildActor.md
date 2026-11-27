# ChildActor 组件

根据类引用生成子 Actor，并按相对偏移跟随宿主 Actor 移动。默认玩法使用该组件在敌人上方显示伤害提示。

`Source.MapActors.Enemy` 声明了 `childActorComp`（类型为 `Source.Components.ChildActorComponent`），因此仅对 Enemy 蓝图及其派生类可用；原生 `Engine.Actor` 不包含该字段。

## 配置步骤

1. 打开 Enemy 蓝图或选中地图上的敌人实例，按[组件配置流程](<概述.md#配置步骤>)在**组件**中打开 `childActorComp`。
2. 将 `className` 设为完整的 Actor 类引用（Enemy 默认为 `Source.MapActors.EnemyDamageText`；蓝图子 Actor 使用如 `Data.Blueprints.Actors.Guard` 的路径）。留空时不生成子 Actor。
3. 设置 `relativePosition`，指定子 Actor 相对于宿主的本地偏移（默认 `[0, 0]`）。
4. 保存蓝图或地图。

**API 参考**：[ChildActorComponent](<../../Lua 与蓝图脚本/源码类/ChildActorComponent.md>) · [Enemy](<../../Lua 与蓝图脚本/源码类/Enemy.md>) · [EnemyDamageText](<../../Lua 与蓝图脚本/源码类/EnemyDamageText.md>)
