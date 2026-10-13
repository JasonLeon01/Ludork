# 原生 UI Adapter

把原生控件加入 Ludork 声明式 UI，使控件可用于游戏运行、预览和 UI 资产编辑器。

## 前置条件

- 一个 C++ Source 项目。
- 一个原生控件类型，以及用于构造、属性赋值、测量、排布与挂载子控件的通用 adapter 操作。
- 已了解[声明式 UI 运行时契约](<../Lua 与蓝图脚本/声明式 UI/运行时契约.md>)。

## 控件描述

系统控件描述定义在 `Engine/Source/Core/include/UI/UiControlAdapterDescriptors.hpp` 中，使用私有 `BIND_UI_CONTROL` X-macro，而不是 Core bindgen 注解。签名如下：

```cpp
BIND_UI_CONTROL(TAG, CONTROL_ID, ADAPTER, DISPLAY_NAME, CATEGORY,
                CHILD_POLICY, SLOT_TYPE, ...PROPERTIES)
```

| 参数 | 含义 |
|---|---|
| `TAG` | 用于特化 `UiControlAdapterTraits` 的唯一 C++ 标记 |
| `CONTROL_ID` | 稳定的系统控件序列化标识符；使用原生类的规范模块根路径 |
| `ADAPTER` | 运行时适配器标识符；使用同一条规范模块根路径 |
| `DISPLAY_NAME` | Palette 标签 |
| `CATEGORY` | Palette 分类 |
| `CHILD_POLICY` | `UiChildPolicy::None`、`Single` 或 `Multiple` |
| `SLOT_TYPE` | `UiControlSlotType::None`、`Canvas` 或 `List` |
| `...PROPERTIES` | 一个或多个属性描述 |

属性使用三种属性描述宏之一。`UI_CONTROL_COMMON_PROPERTY` 适用于所有控件。`UI_CONTROL_PROPERTY` 会被写入并由运行时适配器读取。`UI_CONTROL_EDITOR_PROPERTY` 只对编辑器或预览可用。每个宏接收 `(ID, DISPLAY_NAME, TYPE, REQUIRED, DEFAULT_JSON)`。纯文本控件使用 `UI_CONTROL_PLAIN_TEXT_PROPERTIES(DEFAULT_SIZE)`，富文本控件使用 `UI_CONTROL_RICH_TEXT_PROPERTIES`。这些属性组同时决定编辑器与打包校验所期望的 TextConfig 类型。

导出控件描述前，原生注册表 会校验 ID、属性类型与默认值、属性职责以及 适配器注册情况。

系统控件的两个路径字段必须完全相同。因此 Engine 中绑定的 `Canvas` 在 `CONTROL_ID` 与 `ADAPTER` 上都使用 `Engine.Canvas`。嵌套或缩写标识符无效。

资产 JSON 按 `RuntimeData` 读取，每个属性再按其描述解码为 `UiControlPropertyValue`。工厂函数收到由此得到的 `UiControlProperties` 字典，setter 收到单个带类型的属性值。

Image 的 `drawAs` 使用绑定的 C++ `Image::DrawAs` 枚举，导出名为 `Engine.ImageDrawAs`。JSON 保存成员名，原生解码和运行时 setter 则传递枚举本身。因此，运行时调用 `AssetInstance:setProperty` 时传入 `Engine.ImageDrawAs.Tile`。解码器、属性值 variant、Inspector 的枚举下拉框、ScriptTools 校验与生成声明必须保持一致。

`Engine.WrapBox` 使用 `UiChildPolicy::Single` 和 List Slot。编辑时添加的子节点是重复实例的模板；子节点规则见[UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md>)。

## 步骤

1. 实现原生控件。
2. 在 `LUDORK_UI_CONTROL_DEFINITIONS` 中增加一条 `BIND_UI_CONTROL`。选择稳定的控件 ID，并准确指定子控件规则与 Slot 类型。
3. 用现有的受支持类型与资源处理方式声明属性。新增属性类型还需要同步改动原生解码器、编辑器与 ScriptTools。
4. 在 `UiControlAdapterRegistry` 中注册标记，并提供控件描述所需的构造、赋值、布局和子控件函数。注册表会从控件描述中获取适配器属性 ID。
5. 用平台的 `tools/build_cpp` 入口构建 C++ 项目，以更新预览宿主与控件注册表 并校验 UI 资产。属性类型受支持的控件无须重建编辑器或 ScriptTools。
`Engine.TabView` 是无子节点的 Input 示例：只序列化构造与视觉资源，标签与回调 在运行时绑定。细节见 [UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md>)与 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md>)。

`Engine.CharacterView` 是无子节点的动画 Visual 示例。适配器依据 `size`、纹理/Rect、Actor 缩放、动画、Shader 与 Hue 属性构造原生预览状态。裁切、逐帧推进与特效合成由控件类负责，资源路径转换与属性赋值则由适配器负责。

## 项目预览文件

桌面原生构建会生成 `PreviewHost`、专用的 `PreviewHostRuntime` 库、`EditorCache/Preview.json`，以及 UI 专用的 `EditorCache/UiPreview.registry.json`。源码项目的二进制目录为 `bin/<configuration>`，Standalone 项目使用 `Binaries`。宿主支持地图、Actor、粒子、字幕与 UI 预览。`Preview.json` 描述配套二进制和注册表；重建期间由 `Preview.building` 暂停编辑器连接，发布后重新连接。

Windows 使用 `tools/build_preview_host.bat Game Debug`，macOS 使用 `sh tools/build_preview_host.sh Game Debug` 构建宿主。工具命令统一为 `ScriptTools preview`，例如 `ScriptTools preview validate Game` 校验项目已发布的预览运行时；CMake 开关为 `LUDORK_BUILD_PREVIEW_HOST`。UI 专用的客户端、会话类型和 `UiPreview.registry.json` 保留原名。

Source、Standalone 可编辑项目及模板保留预览能力。最终游戏打包会排除预览可执行文件、专用运行库、清单、构建标记、临时帧和 `Engine/PreviewHost` 源码目录。游戏程序与运行库不链接 `PreviewHostRuntime`。

修改原生 UI 控件后须重新构建项目。配套的预览宿主和控件注册表生成前，预览不可用。更新后的控件如何使用，见[声明式 UI 编写工作流](<../Lua 与蓝图脚本/声明式 UI/编写工作流.md>)。

## 限制

原生控件必须通过项目自己的 Core UI 源码与适配器注册加入，并且满足预览运行时的依赖边界。编辑器插件无法通过 Palette 注册它们。`UiChildPolicy::None` 的控件不能包含子控件，`Single` 最多接受一个子控件。

## 相关页面

- [UI 资产编辑器](<../编辑器用户指南/UI 资产编辑器.md>)
- [UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md>)
- [生成 Metadata 与 Stub](<生成 Metadata 与 Stub.md>)
- [宏参考](<宏参考.md>)
