# Source.Components.ChildActorComponent

`ChildActorComponent` 声明用于生成并挂载子 Actor 的类名与相对位置。可编辑字段 `childActorComp` 定义在 `Source.MapActors.Enemy` 上并由其派生类继承；编辑器中的配置方式见 [ChildActor 组件](<../../编辑器用户指南/Actor 组件/ChildActor.md>)。

**元数据来源**：`Scripts/Source/Components/ChildActorComponent_meta.lua`

## ChildActorComponent

**继承**：`Engine.Component`

### 属性

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `className` | `string` | `""` | 需要实例化的子 Actor 类名。 |
| `relativePosition` | `sf.Vector2f` | `sf.Vector2f(0, 0)` | 相对于所属 Actor 的位置偏移。 |
