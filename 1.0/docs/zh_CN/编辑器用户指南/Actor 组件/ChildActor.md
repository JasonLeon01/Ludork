# ChildActor 组件

## 用途

根据类引用创建子 Actor，并将它放在相对于宿主的位置。默认玩法使用它显示敌人伤害提示。

## 适用范围

默认玩法的 `Source.MapActors.Enemy` 声明了 `childActorComp`，类型为 `Source.Components.ChildActorComponent`；Enemy 蓝图及其派生类继承此字段。原生 `Engine.Actor` 未声明该字段，因此不是所有 Actor 都可直接添加。

## 配置步骤

1. 打开 Enemy 蓝图或选中它的地图实例，按[组件配置流程](<概述.md#配置步骤>)在**组件**中打开 `childActorComp`。
2. 将 `className` 设置为完整的 Actor 类引用。Enemy 默认为 `Source.MapActors.EnemyDamageText`；蓝图子 Actor 可使用 `Data.Blueprints.Actors.Guard` 这样的引用。类名为空时不创建子 Actor。
3. 设置 `relativePosition`，即子 Actor 相对于宿主的本地位置，默认 `[0,0]`。
4. 保存蓝图或地图。需要默认敌人伤害提示行为时，保留默认提示类。

## 参考链接

- [Actor 组件](<概述.md>)
- [ChildActorComponent API](<../../Lua 与蓝图脚本/源码类/ChildActorComponent.md>)
- [Enemy API](<../../Lua 与蓝图脚本/源码类/Enemy.md>)
- [EnemyDamageText API](<../../Lua 与蓝图脚本/源码类/EnemyDamageText.md>)
