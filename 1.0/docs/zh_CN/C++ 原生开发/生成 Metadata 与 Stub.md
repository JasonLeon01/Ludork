# 生成 Metadata 与 Stub

生成文件属于构建产物，不得手工修改。

原生绑定流程用一份声明产出五类结果：

1. 稳定的 `<Module>.<NativeClass>.auto.cpp` 注册单元，以及原生 Lua 模块使用的唯一一份 `<Module>.stub.auto.cpp`；
2. 供 Lua 语言工具使用的 `Scripts/stub/<Module>.d.lua`；
3. 供编辑器使用的 `Scripts/<Module>_meta.lua`；
4. `<Module>.traits.auto.hpp`，内含该模块私有的转换特征；
5. 绑定枚举与带标记常量映射对应的纯模块 `Scripts/Enums/<Module>/<Name>.lua`，类型注解保留在源码中。

这些输出必须在规范模块路径、类名、函数分组与参数顺序上保持一致。Metadata 还携带蓝图执行信息与编辑器信息。

类文件名取自 C++ 限定类型名，与 Lua 的 `name` 选项无关。`<Module>.stub.auto.cpp` 负责注册模块、按绑定顺序调用各类单元，并提供 LuaLS stub 的写入逻辑。

生成的名称与 metadata 收录范围由声明决定，规则见[绑定类](<绑定类.md>)与[函数、事件与执行](<函数、事件与执行.md>)。LuaLS stub 覆盖整个脚本 API，也包括未进入蓝图 Metadata 的成员。

Core bindgen 与 LuaSF 会在 `.d.lua` 中保留多行 `///` 文档，并把 `\param`、`\return` 等已识别的 Doxygen 命令转换为对应的 `@` 形式。

桌面构建通过 `Engine/Tools/NativeStubDump` 调用导出的 `<Module>_write_stub` 完成原生 stub。writer 只为编译后的 `StructTraits` 确认具备独立值语义的类型追加 `copy`、`deepcopy`，不创建 Lua VM。加载前会复制模块运行时依赖；即使模块未重新链接，原生聚合构建也会刷新这些 stub。交叉构建保留源码 stub。生成的转换 traits 只对生成绑定单元强制 include。

## 默认值与类型

| C++ 值类型 | 蓝图 Metadata | LuaLS |
|---|---|---|
| 具名绑定类型 | 完整模块/类型引用 | 限定类型名 |
| 已绑定的原生或 LuaSF 枚举 | `{ enum = "Enums.<Module>.<Name>" }` | 既有的限定枚举类型 |
| `std::vector<T>` 或 `std::array<T, N>` | `{ list = T }` | `T` 数组 |
| 字符串键映射 | `{ dict = T }` | 以字符串为键、值为 `T` 的映射 |
| `std::pair<T1, T2>` 或 `std::tuple<T...>` | `{ tuple = { T1, T2, ... } }` | 固定的数值字段，如 `{ [1]: integer, [2]: string }` |
| `std::variant<T...>` | `{ union = { T1, T2, ... } }` | 各分支组成的联合类型 |
| `std::monostate` | `"nil"` | `nil` |
| `std::optional<T>` | `T` | `T` 或 nil |
| `pure_data` 类型 | `any` | 递归的类型别名 `<Module>.<Name>Value` |
| `std::function` 或 `StrictFunction` | `function` | 声明的回调签名 |

这些映射会递归应用。显式的 metadata 类型 `Pair` 选择的是双分量数值控件，与 C++ 中可容纳不同类型元素的 pair 或 tuple 不是一回事。Lua 转换规则见[运行时值边界](<运行时值边界.md#lua-值转换>)，返回值校验见[函数、事件与执行](<函数、事件与执行.md#声明严格回调>)。

在受支持的情况下，Bindgen 从布尔、数值、字符串或空表的成员初始化器推导默认值。复杂默认值使用[绑定类](<绑定类.md#声明属性>)中的注解花括号语法。variant 的 `{}` 初始化器选择首个声明的分支；默认值有歧义时须显式指定分支。不受支持的显式联合默认值会导致生成失败。

普通非静态方法会生成首个 `self` 参数，其类型为所属模块／类，并设置 `default[1] = "self"`。静态函数与注册事件入口没有接收者参数。该接收者只属于 metadata；C++ 声明、原生 Lua 注册与 LuaLS 冒号调用签名仍保留原有业务参数。

注解中的默认值与参数类型覆盖仍描述原始 C++ 参数。Bindgen 在 metadata 中把它们的位置移至接收者之后，同时保留取值。显式 nil 默认值生成按名称声明的 `defaultUnset`，使对应编辑器输入保持未设置。[Metadata 结构与 Decorator](<../Lua 与蓝图脚本/蓝图脚本/Metadata 结构与 Decorator.md>)说明了生成的结构、带标记的联合类型字面量与编辑器行为。

LuaSF 通过 `LUASF_CALLBACK_CODECS_FILE` 提供具有特殊调用约定的回调别名。Bindgen 在展开规范类型之前按语义别名选择编解码器，并在嵌套容器中保持这一选择。清单缺失、不兼容或规范类型不匹配时，生成或编译会失败。

`BIND_CLASS` 的 singleton 声明还会为已绑定的函数组生成 metadata。包装函数已经选定单例，因此其参数不含 `self`；类方法仍保留有类型的接收者。`Engine.Input` 与 `Engine.Service` 分别是对应的函数组与类接口。

## 生成枚举模块

桌面原生构建使用 `NativeStubDump` 调用 `<Module>_write_enum_catalogue`。编译后的 writer 从实际 C++ 枚举项取得整数值，从带 `BIND_MODULE_PROPERTY(enum = true)` 标记的 const 映射取得标量值，在 `Intermediate` 下写入 catalogue，不创建 Lua VM。随后 `ScriptTools enum-modules` 在 `Enums.Engine`、`Enums.GlobalCore` 或所属原生模块的命名空间下，为每个枚举生成包含带注解的局部常量表及返回语句的独立模块。既有原生 table 导出继续使用同一份源值。

`Enums.sf` 下的 LuaSF 枚举模块来自 `sfml_api.json`。嵌套名称形成目录，例如 `Enums/sf/Keyboard/Scan.lua`；`Scancode` 别名解析为规范 schema `Enums.sf.Keyboard.Scan`。这项生成不会额外公开蓝图节点。

枚举模块自行携带注解，不提供镜像枚举 stub。原生枚举用 `---@class` 保留公开的 `Enums.<Module>.<Name>` 表类型，每个标量常量用行内注解保留原生 API 值类型，例如 `A = (0 --[[@as sf.Keyboard.Key]])`。原生枚举定义仍位于 `Engine.d.lua`、`LuaSF.d.lua` 等所属 API stub 中。值为基础类型的常量映射同样保留 `---@class` 表类型，成员的标量类型直接从值推导。所有形式都只保存一份常量值，并返回该局部表；不使用 `---@meta`，也不构造运行时类。

生成器只写入变化的内容，只清理带有自身生成标记的过期输出，同时清理带有自身标记的旧枚举 stub，并拒绝覆盖同路径的手写文件。编辑器模板与原生缓存包含枚举源码模块，不包含枚举 stub 目录树；其他原生 API stub 仍然保留。游戏包保留枚举源码模块，排除 stub 目录树。

交叉构建与静态 Lua 模块构建需要已有的桌面原生构建产出的枚举源码模块。生成文件缺失时，预检会列出路径并失败，不会猜测 C++ 常量值。修改原生枚举声明或带标记映射后，应先在桌面重新构建，再准备这些构建。

## 重新生成

执行常规 CMake 构建即可新增、更新或移除生成的类单元与模块输出。Bindgen 只写入发生变化的内容，未改动文件的名称与时间戳保持不变。输出看起来没有更新时，先确认带注解的头文件属于该模块配置的公开头文件集合，再查看生成器诊断并重新构建。

即使生成内容未变，被包含的头文件发生改动仍可能重新编译依赖它的类单元。

## 生成 UI View

UI View 与窗口声明由独立生成器 `ScriptTools ui-assets generate <project-root>` 处理。生成文件的归属见[声明式 UI 编写工作流](<../Lua 与蓝图脚本/声明式 UI/编写工作流.md>)。命令与检查见[声明式 UI 校验与打包](<../Lua 与蓝图脚本/声明式 UI/校验与打包.md>)。

## 相关页面

- [绑定类](<绑定类.md>)
- [C++ 绑定故障排除](<C++ 绑定故障排除.md>)
- [函数、事件与执行](<函数、事件与执行.md>)
