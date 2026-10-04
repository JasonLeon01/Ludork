# Source.Components.ChildActorComponent

`ChildActorComponent` 声明子 Actor 的类名与相对位置。

可编辑的 `childActorComp` 字段由默认玩法 Enemy 声明，并由其派生类继承。编辑器配置见 [ChildActor 组件](<../../编辑器用户指南/Actor 组件/ChildActor.md>)。

Metadata 来源：`Scripts/Source/Components/ChildActorComponent_meta.lua`

## ChildActorComponent

直接 metadata 基类：`{ "Engine", "Component" }`

### 属性

| 名称 | 类型 | 默认值 | Metadata |
|---|---|---|---|
| `className` | `string` | `""` | — |
| `relativePosition` | `sf.Vector2f` | `{ [1] = 0, [2] = 0 }` | — |

### 函数与事件

未声明任何 Blueprint 函数或事件。
