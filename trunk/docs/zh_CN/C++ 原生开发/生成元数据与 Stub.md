# 生成元数据与 Stub

原生绑定管线会一次性解析 C++ 声明宏，统一生成 Lua 胶水代码、LuaLS 类型声明、蓝图元数据、私有转换特征以及枚举模块。

> **重要**：生成文件属于构建产物。如需修改，请编辑 C++ 声明并重新构建，切勿手工修改生成文件。

## 管线输出

每个模块产出五类结果，它们在规范模块路径、类名、函数分组与参数顺序上严格一致（元数据还会附带蓝图执行与编辑器信息）：

1. 稳定的 `<Module>.<NativeClass>.auto.cpp` 类注册单元，以及原生 Lua 模块对应的唯一一份 `<Module>.stub.auto.cpp`。
2. 供 Lua 语言工具使用的 `Scripts/stub/<Module>.d.lua`（覆盖完整脚本 API，包括 `metadata = false` 的成员）。
3. 供编辑器蓝图目录使用的 `Scripts/<Module>_meta.lua`。
4. 包含模块私有转换特征的 `<Module>.traits.auto.hpp`。
5. 为绑定枚举与标记常量映射生成的纯模块 `Scripts/Enums/<Module>/<Name>.lua`（附带内联注解）。

类单元文件名由 C++ 限定类型名推导，不受 Lua `name` 选项影响。`<Module>.stub.auto.cpp` 负责注册模块、按绑定顺序调用各类单元并提供 LuaLS Stub 写入器。生成名称与元数据收录规则见[绑定类](<绑定类.md>)与[函数、事件与执行](<函数、事件与执行.md>)。

Core bindgen 与 LuaSF 会在 `.d.lua` 中保留多行 `///` 注释，并将已识别的 Doxygen 命令（如 `\param`、`\return`）转换为对应的 `@` 标签。

桌面构建通过 `Engine/Tools/NativeStubDump` 调用导出的 `<Module>_write_stub` 符号完成原生 Stub 生成，全程无需创建 Lua VM。写入器仅在编译后的 `StructTraits` 确认类型具备独立值语义时才追加 `copy` 与 `deepcopy`。

加载模块前会自动复制其运行时依赖；即使模块本身未重新链接，原生聚合构建也会刷新 Stub（交叉构建则保留源码 Stub）。生成的转换特征仅通过绑定 PCH 提供给生成绑定单元，禁用 PCH 时改为强制包含；详见[构建项目](<构建与模块布局.md#构建项目>)。

## 默认值与类型

| C++ 值类型 | 蓝图元数据 | LuaLS |
|---|---|---|
| 具名绑定类型 | 完整模块／类型引用 | 限定类型名 |
| 已绑定的原生或 LuaSF 枚举 | `{ enum = "Enums.<Module>.<Name>" }` | 既有的限定枚举类型 |
| `std::vector<T>` 或 `std::array<T, N>` | `{ list = T }` | `T` 数组 |
| 字符串键映射 | `{ dict = T }` | 以字符串为键、值为 `T` 的字典 |
| `std::pair<T1, T2>` 或 `std::tuple<T...>` | `{ tuple = { T1, T2, ... } }` | 固定数值字段（如 `{ [1]: integer, [2]: string }`） |
| `std::variant<T...>` | `{ union = { T1, T2, ... } }` | 各分支组成的联合类型 |
| `std::monostate` | `"nil"` | `nil` |
| `std::optional<T>` | `T` | `T` 或 `nil` |
| `pure_data` 类型 | `any` | 递归类型别名 `<Module>.<Name>Value` |
| `std::function` 或 `StrictFunction` | `function` | 声明的回调签名 |

上述映射均递归生效。显式元数据类型 `Pair` 对应编辑器中的双分量数值控件，与可存放异构元素的 C++ pair 或 tuple 不同。运行时转换规则见 [Lua 值转换](<运行时值边界.md#lua-值转换>)，回调结果校验见[声明严格回调](<函数、事件与执行.md#声明严格回调>)。

- **默认值**：在受支持的范围内，Bindgen 会从布尔、数值、字符串与空表成员初始化器推导默认值；复杂默认值采用[声明属性](<绑定类.md#声明属性>)中的花括号语法。`std::variant` 的 `{}` 初始化器默认选取首个分支，存在歧义时请显式指定分支；不受支持的显式联合默认值会导致生成失败。
- **元数据 `self` 接收者**：普通非静态方法会自动获得仅存在于元数据中的首个 `self` 参数，其类型为所属模块／类且 `default[1] = "self"`。静态函数与注册事件入口不包含该参数。C++ 声明、原生 Lua 注册与 LuaLS 冒号调用签名均保持原始业务参数。
- **参数与 `nil` 对齐**：注解中的 `defaults` 与 `parameter_types` 均描述 C++ 业务参数，Bindgen 会在元数据中自动将其索引顺移至 `self` 之后并保留取值。显式 `nil` 默认值会输出具名的 `defaultUnset` 条目，使对应编辑器输入框保持未设置状态。Schema 结构、带标记的联合字面量与编辑器行为见[元数据结构与 Decorator](<../Lua 与蓝图脚本/蓝图脚本/元数据结构与 Decorator.md>)。
- **单例函数组**：标记为单例的 `BIND_CLASS` 同时为其绑定函数组生成元数据。函数组包装器已隐含目标单例，其元数据参数不含 `self`，而类方法仍保留带类型的接收者（`Engine.Input` 与 `Engine.Service` 即采用这一划分）。
- **LuaSF 回调编解码器**：LuaSF 通过 `LUASF_CALLBACK_CODECS_FILE` 提供具有特殊调用约定的回调别名。Bindgen 在展开规范类型前先按语义别名匹配编解码器，并在嵌套容器中保持该选择。清单缺失、版本不兼容或规范类型不匹配都会导致生成或编译失败。

## 生成枚举模块

桌面原生构建通过 `NativeStubDump` 调用 `<Module>_write_enum_catalogue`（不创建 Lua VM），读取实际的 C++ 枚举项整数值以及标记了 `BIND_MODULE_PROPERTY(enum = true)` 的 `const` 映射标量值，并在 `Intermediate` 下写出枚举目录。随后 `ludork-codegen enum-modules` 在 `Enums.Engine`、`Enums.GlobalCore` 或所属模块命名空间下，为每个枚举生成带注解的局部常量表与返回语句，与原生表导出保持一致。

- **LuaSF 枚举**：`Enums.sf` 下的模块由 `sfml_api.json` 生成。嵌套名称映射为子目录（例如 `Enums/sf/Keyboard/Scan.lua`），`Scancode` 别名解析至 `Enums.sf.Keyboard.Scan`。生成过程不会新增蓝图节点。
- **注解格式**：枚举模块自带完整类型注解，不生成镜像 Stub 文件。原生枚举通过 `---@class` 声明 `Enums.<Module>.<Name>` 表类型，每个标量常量附带原生 API 类型注解（如 `A = (0 --[[@as sf.Keyboard.Key]])`），而原生枚举定义仍位于 `Engine.d.lua`、`LuaSF.d.lua` 等所属 Stub 中。基础类型常量映射同样使用 `---@class` 表类型，成员标量类型直接由值推导。每个模块仅保存一份常量并返回该局部表，不使用 `---@meta`，也不构造运行时类。
- **所有权与打包**：生成器仅在内容变化时写入文件，且仅清理带有自身生成标记的过期输出与废弃枚举 Stub，若与手写文件冲突则直接拒绝。编辑器模板与原生缓存随附枚举源码模块但不含枚举 Stub 目录树（保留其他原生 API Stub）；游戏发行包包含枚举源码模块并排除 Stub 目录树。
- **交叉构建要求**：交叉构建与静态 Lua 模块构建依赖前置桌面原生构建生成的枚举源码模块。文件缺失时预检会报告缺失路径并失败，而不会猜测常量值；修改原生枚举或带标记映射后，请先在桌面端重新构建。
- **项目自有枚举**：通用数据、动画与粒子文档会自动生成项目枚举。原生元数据可通过 `metadata_type = { enum = "Enums.Particle", valueType = "string" }` 引用这些枚举，而不改变 C++ 字符串参数或 Lua API 值。详见[通用数据与文本配置](<../编辑器用户指南/通用数据与文本配置.md>)。

## 重新生成

执行常规 CMake 构建即可新增、更新或移除生成的类单元与模块输出。Bindgen 在所有平台上均按区分大小写的路径顺序、统一使用 `/` 分隔符扫描公开头文件，输出无 BOM 的 UTF-8 文本与 LF 换行符。

- **原子写入与标记保护**：写入前 Bindgen 会逐字节比对内容，未变化文件保持原有修改时间不变（重新生成已有 CRLF 文件时会规范化为 LF）。内容变更时先在目标路径旁写入临时文件再执行原子替换，保留原文件权限并在失败时保持旧文件完好。带生成标记的输出绝不覆盖手写文件，过期绑定单元亦仅在验证标记后删除。
- **完成标记**：每次成功生成后，绑定完成标记（bindings stamp）都会更新，即使所有输出字节均无变化，以便通知 CMake 输入已处理完毕而不搅动生成文件时间戳。被包含的头文件变动时，依赖它的类单元仍可能触发重新编译。
- **排查过期输出**：若生成结果疑似未更新，请确认带注解的头文件已纳入模块配置的公开头文件集合，检查生成器诊断输出后重新构建。

## 生成 UI View

UI View 与窗口声明由独立生成器 `ludork-assets ui-assets generate <project-root>` 负责生成。文件归属与导出步骤见[声明式 UI 编写工作流](<../Lua 与蓝图脚本/声明式 UI/编写工作流.md>)。
