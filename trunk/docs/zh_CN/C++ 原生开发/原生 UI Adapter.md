# 原生 UI Adapter

将原生 C++ 控件纳入 Ludork 的声明式 UI 契约，使运行时、PreviewHost 与编辑器控件注册表共享同一份描述符定义。本流程适用于 C++ Source 项目，阅读前建议先熟悉[声明式 UI 运行时契约](<../Lua 与蓝图脚本/声明式 UI/运行时契约.md>)。

## 控件描述符契约

系统控件描述符集中定义在 `Engine/Source/Core/include/UI/UiControlAdapterDescriptors.hpp`，采用私有的 `BIND_UI_CONTROL` X-macro（而非 Core bindgen 注解）：

```cpp
BIND_UI_CONTROL(TAG, CONTROL_ID, ADAPTER, DISPLAY_NAME, CATEGORY,
                CHILD_POLICY, SLOT_TYPE, ...PROPERTIES)
```

| 参数 | 含义 |
|---|---|
| `TAG` | 用于特化 `UiControlAdapterTraits` 的唯一 C++ 标签 |
| `CONTROL_ID` | 稳定的系统控件序列化标识符，必须为原生类的规范模块根路径 |
| `ADAPTER` | 运行时 Adapter 标识符，必须与 `CONTROL_ID` 一致 |
| `DISPLAY_NAME` | Palette 中的显示标签 |
| `CATEGORY` | Palette 中的所属分类 |
| `CHILD_POLICY` | `UiChildPolicy::None`、`Single` 或 `Multiple` |
| `SLOT_TYPE` | `UiControlSlotType::None`、`Canvas` 或 `List` |
| `...PROPERTIES` | 一个或多个属性描述符 |

对于每个系统控件，`CONTROL_ID` 与 `ADAPTER` 必须填写完全相同的规范路径（例如 `Engine` 中绑定的 `Canvas` 对应 `Engine.Canvas`）；嵌套路径或缩写标识符均无效。导出描述符前，原生注册表会自动校验 ID、属性类型与默认值、属性职责以及 Adapter 注册完整性。

### 属性描述宏

每条属性描述接收 `(ID, DISPLAY_NAME, TYPE, REQUIRED, DEFAULT_JSON)`，使用以下三个宏之一：

| 宏 | 作用范围 |
|---|---|
| `UI_CONTROL_COMMON_PROPERTY` | 应用于全部控件的公共属性 |
| `UI_CONTROL_PROPERTY` | 持久化序列化并由运行时 Adapter 读取应用的属性 |
| `UI_CONTROL_EDITOR_PROPERTY` | 仅限编辑器与预览使用的属性 |

纯文本控件使用 `UI_CONTROL_PLAIN_TEXT_PROPERTIES(DEFAULT_SIZE)`，富文本控件使用 `UI_CONTROL_RICH_TEXT_PROPERTIES`。这些属性组同时决定了编辑器与打包校验所要求的 `TextConfig` 类型。

### 解码与特殊控件

资产 JSON 先读取为 `RuntimeData`，随后每个属性依据其描述符解码为 `UiControlPropertyValue`。工厂函数接收解码后的 `UiControlProperties` 字典，Setter 接收单个带类型的属性值。

- **枚举属性（`Image::DrawAs`）**：Image 的 `drawAs` 使用已绑定的 C++ `Image::DrawAs` 枚举（导出名为 `Engine.ImageDrawAs`）。JSON 持久化成员名称，原生解码与运行时 Setter 直接传递枚举值本身（因此 `AssetInstance:setProperty` 应传入 `Engine.ImageDrawAs.Tile`）。请保持解码器、属性值 variant、Inspector 枚举选择器、Toolchain 校验与生成声明同步一致。
- **模板子节点（`Engine.WrapBox`）**：采用 `UiChildPolicy::Single` 与 List Slot，将其唯一编辑期子节点视为重复生成实例的模板。详见 [UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md>)。

## 注册原生控件

1. 实现原生控件类型，并补充构造、属性赋值、测量、排布与子节点挂载所需的通用 Adapter 操作。
2. 在 `LUDORK_UI_CONTROL_DEFINITIONS` 中新增一条 `BIND_UI_CONTROL` 条目，指定稳定的规范控件 ID、子节点策略与 Slot 类型。
3. 使用现有受支持的属性类型与资源处理方式声明属性。如需引入新属性类型，须同步更新原生解码器、编辑器与 Toolchain。
4. 在 `UiControlAdapterRegistry` 中注册该标签，提供描述符所需的工厂、Setter、排布器与子节点回调。注册表会自动从描述符复制 Adapter 属性 ID。
5. 运行对应平台的 `tools/build_cpp` 脚本构建 C++ 项目，以刷新 PreviewHost、更新控件注册表并校验 UI 资产。仅使用现有属性类型的控件无需重新构建编辑器或 Toolchain。

### 内置参考 Adapter

- **`Engine.TabView`（无子节点的 Input 控件）**：仅序列化构造参数与视觉资源，标签文本与回调在运行时绑定。详见 [UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md>)与[交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md>)。
- **`Engine.CharacterView`（无子节点的动画 Visual 控件）**：依据 `size`、纹理与 `Rect`、Actor 缩放、动画、材质引用及实例参数构建原生预览状态。裁切、帧推进与特效合成由控件类负责，资源路径转换与属性赋值由 Adapter 负责。

预览构建产物、注册表发布与打包规则见 [PreviewHost：构建、发布与排错](<../PreviewHost/构建、发布与排错.md>)。

> **注意**：原生控件必须位于项目的 Core UI 源码中并满足预览运行时的依赖边界；编辑器插件无法向 Palette 注册原生控件。声明为 `UiChildPolicy::None` 的控件不能包含子控件，`UiChildPolicy::Single` 最多包含一个子控件。
