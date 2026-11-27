# Actor、敌人、物品与装备

本页介绍默认玩法中地图 Actor、可见性、移动与危险寻路、敌人战斗、可收集物品以及装备的运行时规则。各项目可按需扩展或替换这些默认战斗与背包规则。

## Actor 与 Character

`Actor` 提供地图归属、网格/像素坐标、生命周期钩子与碰撞行为；Character 子类在此基础上增加移动与朝向。公开的类默认值应直接定义在 Lua 类表上，`init` 仅用于构造参数与私有运行时状态。

- **材质**：通过 `material` 引用 `Data/Materials/...json`，通过 `materialParameters` 保存按实例独立的参数覆盖（允许多个 Actor 共享同一材质资产并使用不同参数）。请先设置 `material` 再设置参数；运行时修改若需跨存档保留，须显式记录状态。材质不会改变移动速度。参见[材质资源](<../全局与 Core 模块/Core Modules/Engine/地图与材质.md#材质资源>)。
- **可见性（`Actor.visible`）**：默认为 `true`。直接赋值仅更新当前 Actor（`getVisible()` 读取该值）；`setVisible(value, applyToChildren = true)` 可向子代传播。自身或祖先隐藏的 Actor 不会渲染、遮挡光线、碰撞、触发重叠（overlap）、占用寻路格或响应地图点击交互，隐藏的敌人也不贡献移动危险或进入怪物手册列表。修改可见性会刷新受影响的缓存，同时保留实例、维持 `Tick` 活跃，并保持碰撞与动画设置不变。
- **粒子发射器（`emitterComp`）**：配置 `Engine.EmitterComponent` 可附加[通用粒子](<../../编辑器用户指南/通用粒子.md>)特效，特效跟随 Actor 变换并按列表顺序紧接宿主前后绘制。隐藏或休眠的 Actor 会冻结 GPU 播放，同时保留状态与手动暂停设置；销毁、世界逐出、地图替换与场景退出会释放特效。
- **公告板与子 Actor**：通过 `billboardComp`（[`Engine.BillboardComponent`](<../全局与 Core 模块/Core Modules/Engine/玩法类型.md#billboardcomponent>)）在玩家靠近时显示头顶图像或文本，通过 `childActorComp` 挂载嵌套子 Actor（如 `Enemy` 伤害读数，参见 [ChildActor 组件](<../../编辑器用户指南/Actor 组件/ChildActor.md>)）。编辑器组件配置（含光源）见 [Actor 组件](<../../编辑器用户指南/Actor 组件/概述.md>)。

### 条件出现

[`Source.MapActors.ConditionalActor`](<../源码类/ConditionalActor.md>) 新增 `conditionVariable`、`conditionOperator` 与 `conditionValue`，根据已声明的游戏变量自动切换可见性：

- **配置方式**：选定已声明的数值、布尔或字符串变量、比较运算符及目标值（编辑器按变量类型自动选择输入控件）。变量名为空时禁用条件控制。数值支持 `==`、`~=`、`>`、`>=`、`<` 与 `<=`，布尔与字符串支持 `==` 与 `~=`。非法数据直接抛出错误，不做隐式类型转换。
- **继承范围**：`Enemy`、`Item`、`Equip`、`DoorBase`、`Teleporter` 以及默认玩法的直接地图 Actor 蓝图均继承 `ConditionalActor`。`Bottle` 与 `Gem` 采用 Script Mixin，`EnemyDamageText` 保持自身的提示与 `Tick` 策略。
- **订阅生命周期**：`onCreate` 完成订阅并立即求值；后续变量写入会动态切换可见性，隐藏中的 Actor 仍持续监听。`onDestroy` 与大地图休眠会取消订阅，大地图唤醒则重新订阅并求值。重写生命周期事件时务必链接父类调用（图模式蓝图需在执行路径上保留[父事件调用](<../蓝图脚本/执行流、事件与变量.md#父事件调用>)）。

地图加载与生命周期顺序见 [SceneMap](<../源码类/SceneMap.md>)，条件订阅与持久化见 [ConditionalActor](<../源码类/ConditionalActor.md>)。

### 组合式大地图中的 Actor

- **统一注册表**：`WorldGameMap` 维护单一世界坐标 Actor 注册表；跨越已就绪子地图边界时，同一实例会重新归属新子地图。空洞属于大地图的一部分：带碰撞的 Actor 无法进入未加载的摆放区，无碰撞的根 Actor 则进入休眠并异步请求未加载区域。
- **区块激活**：仅位于 `Active` 状态 32×32 格区块中的根 Actor 参与模拟与渲染。离开区块时触发 `onWorldSleep()`，返回时触发 `onWorldWake(elapsedSeconds)`，两者均不替代 `onCreate` 或 `onDestroy`。从未激活过的 `Prepared` 根 Actor 不接收生命周期回调，也不进入玩法查询结果。
- **绘制顺序与图层**：图块与 Actor 按清单的 `layerOrder` 绘制，仅运行时图层按首次使用顺序追加，玩家在所属图层中始终最后绘制。隐藏子地图的图块图层会同步将对应 Actor 从颜色与光照表面中隐藏。
- **查找与屏蔽**：`getActorByTag` 在不加载缺失子地图的前提下查找 `Active` Actor 及激活后休眠的驻留根 Actor；被逐出的 Actor 在重新加载前保持缺失。`SceneMap:recordDestroyedActorTag` 会在驻留数据及未来子地图载荷中屏蔽该标签且不加载缺失子地图（阻止生命周期执行、查找或在逐出后重现），而 `recordDestroyedActor` 仅负责写入记录。

## 移动、寻路与动画

脚本[移动与寻路](<../节点函数/Movement.md>)与[门动画](<../源码类/DoorBase.md>)均返回 `Engine.AsyncOperation`，供 Lua 与蓝图等待完成。

### 到格、Dir4 寻路与点击目标

- **停止与到格**：停止移动（包括打开 UI 时暂停移动）只要导致玩家吸附到新格子，就会执行到格流程，避免跳过重叠交互与移动步效果。危险变更触发的点击路线重规划与立即完成路线均先退回最后一个完整格子再停止；立即完成路线随后统一应用一次目的地重叠交互与收集到的移动路径。
- **Dir4 与占格范围**：A* 与实际移动执行同一条 Dir4 边缘检查——每个新进入的前导格都必须允许从来源图块离开，并允许从目标图块反方向进入。通行性与寻路将 Actor 占格范围精确落于查询格而不带入进行中的像素偏移，使危险直线检查不受移动方向和进度影响。
- **点击回退与绕行重叠**：点击移动对常规目标和单格受阻目标直接使用 A*；若 Actor 占格范围阻塞直接路线，则回退搜索相邻格。碰撞目标执行最后一步碰撞交互，无碰撞的不可通行目标则停在相邻格。寻路在途经时会绕开无碰撞但具有可执行 `onOverlap` 的 Actor，但显式点击此类 Actor（如 Teleporter）会保留直达该格的路线并触发 `onOverlap`。
- **组合式大地图**：大地图寻路可跨越已加载子地图边界，并将未加载子地图视为不可通行，不触发隐式 I/O。空洞可通行，顶层材质为 `nil`。

### 地图视口（`GameMap.MapViewRect`）

`GameMap.MapViewRect` 定义逻辑地图画布（默认 `sf.IntRect.new(192, 32, 416, 416)`），同时决定裁剪绘制区与摄像机视口尺寸。小于画布的地图轴自动居中，大于画布的轴限制在可滚动范围内。地图渲染、指针坐标换算、世界/UI 定位、动画、光照、雾效、远景与天气共用该矩形，矩形外的指针输入会被忽略。

### 移动危险（`Domain`、`Flank`、`Blockade`）

- **空间规则**：`Domain`、`Flank` 与 `Blockade` 授予 `Gameplay.Movement.Hazard`。实际移动与 `MovementDangerGrid` 统一查询 `MovementSpecials.Preview`（派发 `Event.Movement.QueryHazard`），保证空间逻辑一致。`Domain` 覆盖曼哈顿距离 `<= range` 的格子（最小为 `1`，因此 `Domain = 1` 仅涵盖上下左右四个正交邻格）并遵循战斗特殊能力（如 `Magic` 无视防御）。`Flank` 要求敌人位于相对的相邻两侧，`Blockade` 影响相邻格；正数伤害累加。
- **提示与点击路线**：玩家持有怪物手册后，共享的[共享玩法配置](<运行时数据、配置与存档.md#共享玩法配置>)伤害提示设置决定是否显示敌人伤害读数与危险格合计伤害（不影响玩法逻辑）。即使隐藏提示，点击路线仍会避开正危险格（显式点击的目的地除外）。点击 `Domain` 或 `Blockade` 敌人仅忽略该目标自身的贡献（含其在 `Flank` 中的一侧）；玩家不会占用的碰撞与重叠目标忽略目的地格的危险。
- **按需危险网格**：网格仅在需要地图提示或点击寻路时物化，扫描 `Event.Movement.QueryHazardBounds` 覆盖范围内的格子（参见 [MovementSpecials](<../源码类/Gameplay.md#movementspecials-与移动危险>)），并仅通过 `Preview` 查询相关敌人。区域平移时复用重叠格结果（含零伤害格），普通地图复用稳定缓存，单格查询保持局部计算。
- **缓存失效与重规划**：区块激活/休眠及敌人或能力修改会使受影响的危险缓存失效。仅当危险修订影响剩余路径或可进入的敌人目标时，活动移动才会失效并退回最后一个完整格子重新规划。

## 敌人

Enemy 蓝图引用一个 Enemy 通用数据成员。`GeneralDataTypes.Create` 将其复制为 `EnemyAttributeSet`，与 `AbilitySystemComponent` 一同暴露为 `Battler.attributes`。Enemy 通用数据不含成员事件，战斗逻辑由 Gameplay 能力与效果驱动。

### 属性与特殊能力

- **生命值与成长**：`MAXHP` 是敌人的虚拟耐久值（敌人无可变 `HP`）。玩家 `HP` 位于 `PlayerAttributeSet`，在 Player 通用数据中初始为 `1000`，不作为蓝图属性覆盖。应用修饰项后玩家当前 `HP` 限制在 `0..MAXHP`；永久 `MAXHP` 与 `LEVEL` 成长仅响应 Base 变化，确保临时 Infinite 效果不改变永久成长。
- **敌人 `special` 条目**：保存整数次数/范围、仅表存在的 `""`、`Vampire` 比率、`FixDmg` 数值/表达式以及 `Reborn` 类路径。每项转化为带对应标签与能力的 Infinite `Special.<id>` 效果（战斗特殊能力挂钩战斗 Gameplay 事件；移动特殊能力使用 `Event.Movement.QueryHazard` 与 `Gameplay.Movement.Hazard`）。

### 战斗结算、伤害预览与击败处理

- **战斗计算**：每个敌人都授予 `Ability.Combat.MotaBattle`。其纯计算产出伤害明细与数值型 `Enums.BattleResultCode`（`WIN`、`CANNOT_DAMAGE`、`LETHAL_COUNTER_DAMAGE`）。伤害预览无副作用复用该计算；碰撞流程则在调用 `CommitResult(result)` 前校验伤害、Game Over、奖励与状态效果 Spec。
- **攻击时序与 Game Over**：反击伤害通过场景计时器在敌人攻击动画最早的 `dmg` 时间标签处弹出（无标签或无动画时立即弹出）。生命值立即扣除，后续结算等待双方攻击动画播完。致命移动危险（`Domain`/`Flank`/`Blockade`）伤害同样立即生效，并在并发启动的最长攻击动画结束后进入 Game Over。
- **击败结算顺序**：获胜且双方攻击动画完成后，先运行 Enemy Actor 蓝图的 `onDefeat` 事件（Enemy 通用数据无 `onDefeat` 图），完成后按固定顺序执行结算：应用 `afterBattleVarChanges` → 记录并销毁敌人 → 生成 `drops` 或 `Reborn` 替代者 → 发放奖励与状态。[击败粉碎特效](<资源、音频、视频与特效.md#着色器雾与转场>)纯属视觉效果；被销毁的敌人立即停止阻挡移动与光线。
- **掉落与 `Reborn`**：`drops` 将 Item 蓝图路径映射到 `sf.Vector2i` 格偏移，在敌人所在图层生成、作为新增 Actor 持久化，并通过 `GeneralDataGraphAbility` 激活通用数据的 `onDrop` 图（重复摆放标签会被跳过；无效或非 Item 路径会抛出错误）。`Reborn` 以其非空蓝图类路径在同格同层生成替代者并分配新地图摆放标签，正常执行 `onCreate` 并记录为新增 Actor，确保地图重建与读档保持原敌人已销毁且仅恢复一次替代者（无效路径会失败）。
- **`afterBattleVarChanges`**：为已声明的 `int`/`float` 变量保存 `variableName = { operator, operand }`，支持 `=`、`+`、`-`、`*`、`/`、`//`、`%`、`**`（序列化为值为二元素数组的 JSON 对象）。运算在战斗扣血前校验，并在 `onDefeat` 后通过固定 `Engine.Eval` 表达式与全新局部变量基于实时变量重新求值——存储的运算符绝不会作为源码执行。
- **伤害预览公式**：
  - 无回复时，敌人最大生命 `H` 与正数单次伤害 `D` 对应 `max(ceil(H / D) - 1, 0)` 次反击。
  - 带有 `Vampire = R` 时，每次非终结反击回复 `floor(counterDamage * R)`。若非一击必杀满足 `D <= healing`，玩家无法取胜；否则反击次数为 `ceil((H - D) / (D - healing))`。
  - `First` 在玩家首次攻击前额外增加一次敌人攻击（满血敌人无法借此回复）。
  - `FixDmg` 直接使用数字字面量，或先代入实时 `{m<attribute>}` 与 `{e<attribute>}` 值求值表达式，对有限非负结果向下取整后加在回合与先攻伤害之后。

## 物品

- **成员图（`onUse` 与 `onDrop`）**：作为显式激活的 `GeneralDataGraphAbility` 图运行，而非 Actor 事件。`onUse` 以玩家为目标（玩家状态节点应置于此处；`EnemyBook` 通过 [`GlobalFunctions.Scene.ShowEnemyBook`](<../节点函数/Scene.md#showenemybook>) 打开怪物手册），`onDrop` 以生成的 Item Actor 为目标。由于图所属对象为 `GameplayEventData`（公共函数中亦然），调用 Player 或 Item 方法时请将 `GameplayEventData.target` 连至可见的 `self` 引脚。
- **重叠物品拾取**：最高占用图块图层上的碰撞回调按视觉从上到下的顺序执行（图层中排在后面的条目先绘制、先触发）。撞向重叠可拾取物品的受阻步进会拾取最上方物品并中止本次碰撞派发；后续步进（或跨固定更新持续按住方向键）会逐一拾取剩余物品。
- **可复用传送道具节点**：`GlobalFunctions.Mota.CenterSymmetricTeleport`、`GoUpstairsSamePos` 与 `GoDownstairsSamePos` 分别支撑 `CenterSymmetricTeleport`、`UpstairsTransportation` 与 `DownstairsTransportation`。相邻楼层节点遵循当前区域地图顺序，在传送前按玩家零起始格坐标校验持久化的目标地图；中心对称传送以当前地图的 `(width - 1 - x, height - 1 - y)` 为目标。每个节点暴露 `Success` 与 `Failed` 分支；失败时以通用提示显示本地化 `FLY_FAIL` 且不消耗物品——请仅将 `GlobalFunctions.Player.RemoveItem` 连到 `Success` 分支。

## 装备

- **属性加成与成员图**：装备的 `attrPlus` 作为装备槽来源的 Infinite 效果在 `onEquip` 图运行前应用，并在 `onUnequip` 运行前移除。槽位名称必须与当前职业（Class）定义匹配。
- **状态（State）**：状态是携带 `State.<id>`、叠加层数、属性修饰项与能力的 Infinite 效果。`Weak` 每层降低 `1` 点 ATK/DEF（下限为 `0`），HUD 上显示当前值与虚弱层数（如基础 `10` 且叠加两层时显示 `8 (-2)`）。`Poisoned` 通过 `Event.Combat.ResolveIncomingDamage` 增加伤害。状态图订阅 `Event.Movement.Step`（`onWalk`）或 `Event.State.Trigger.<id>`（显式钩子）。

## 运行时引用

请将通用数据标识符视为稳定、与语言无关的键：职业、动画与装备槽键均原样保存和比对。显示名称与说明可使用花括号包裹的本地化键——仅在展示时通过 `LOC` 解析，并在语言变更时刷新（参见[游戏本地化工作流](<../../插件开发与安装/官方插件/游戏本地化工作流.md>)）。重命名任意标识符后必须同步更新所有已保存引用。

完整数据库 Schema 与成员列表可从 [Class](<../../编辑器用户指南/通用数据参考/Class.md>) 开始查阅；常见用法示例可从[消耗品](<常见用法/消耗品.md>)入手。
