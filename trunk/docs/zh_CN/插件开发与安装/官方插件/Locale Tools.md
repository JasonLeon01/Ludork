# Locale Tools

官方插件 Locale Tools（源码位于 `Plugins/OfficialLocaleTools`）用于处理游戏翻译工作簿，同时也是普通菜单命令、文本提示提供器、Export Hook 与 before-Pack 排除 Hook 的参考实现（不注册地图项右键菜单命令）。

请通过[发行版官方插件](<../安装与管理插件.md#发行版官方插件>)完成安装，并在**插件管理**中确认其状态为**已加载**。

## 注册

公开入口类型会加载插件自身的 `locale.json`，随后注册以下四个组件：

- 一条排序值为 `900` 的**数据库**菜单命令（**打开 Locale 文件**），用于打开 `Data/Locale/Locale.xlsx`
- 用于编辑器 `{ID}` 实时提示的 `LocaleTextHintProvider`
- 通过 `RegisterBeforeExportHook` 注册的 `LocaleExportHook`
- 在生成最终游戏发行包时排除创作工作簿的 before-Pack Hook（Standalone 项目转换会跳过该 Hook，从而保留工作簿）

`LocaleExportHook` 将 `Data/Locale/Locale.xlsx` 声明为输入、将生成的 Lua 语言文件声明为输出以参与导出变更检测，并在每次导出尝试后刷新自身缓存与编辑器文本提示。

关于工作簿格式、生成的 Lua 文件与运行时调用方式，请参阅[游戏本地化工作流](<游戏本地化工作流.md>)。

## 菜单行为

菜单处理函数要求当前已打开项目，确认 `Data/Locale/Locale.xlsx` 存在后交由操作系统使用默认电子表格应用打开，并通过 `IPluginUserInterface` 报告已处理的警告与错误。

## 插件语言与游戏语言

`Plugins/OfficialLocaleTools/locale.json` 依据 `EditorLanguage` 本地化插件自身的菜单标签与诊断消息，与项目的游戏语言目录完全独立。
