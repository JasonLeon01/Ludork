# Source.Battler

`Battler` 是 `Enemy` 与 `Player` 的 Lua 玩法基类。各具体子类会依据通用数据构造生成的 Attribute Set，并持有用于管理属性、能力、效果与标签的 Ability System Component。

**元数据来源**：`Scripts/Source/Battler_meta.lua`

## 运行时状态

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `attributes` | `GlobalCore.AttributeSet` | — | 运行时生成的 Attribute Set；具体子类会将其类型收窄为 Enemy 或 Player。 |
| `_abilitySystemComponent` | `GlobalCore.AbilitySystemComponent` | — | 属性、能力、效果与标签的私有持有者。 |

## 运行时 API

实例方法省略隐式接收者 `self`（`Source.Battler`）。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `init` | `attributes: GlobalCore.AttributeSet` | — | 接收一个 Attribute Set，并创建对应的 Ability System Component。 |
| `getAbilitySystemComponent` | — | `GlobalCore.AbilitySystemComponent` | 返回当前的 Ability System Component。 |
| `getAttr` | `name: string`, `base?: bool = false` | `number` | 返回数值属性的当前值（Current）；`base` 为 `true` 时返回基础值（Base）。 |
| `setAttr` | `name: string`, `value: number` | — | 通过 Ability System 设置数值属性的基础值，并重新计算当前值。 |
| `addAttr` | `name: string`, `delta: number` | — | 为数值属性的基础值增加 `delta`；传入负数表示减少。 |
| `playAttackAnimationAt` | `scene`, `targetPosition` | `GlobalCore.Animation?` | 在目标位置播放 `attributes.ANIMATION_KEY` 并返回动画对象；键为空时返回 `nil`。调用方可读取其视觉时长与时间标签。 |

读写单个数值属性时应使用 `getAttr`、`setAttr` 和 `addAttr`。`addAttr` 基于 Base 计算，不会把生效中的修饰效果写入基础值。这三个方法会执行原生校验：属性名未知、数值非有限数，或与生成的 `int` / `float` 类型定义不符时均会失败。其余字段可直接从 `attributes` 读取；批量写入、持久化、效果应用与变更监听则应通过 Ability System 完成。
