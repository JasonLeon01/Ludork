# Official Locale Tools

`Plugins/OfficialLocaleTools` 是普通菜单命令、文本提示提供器，以及 Export Hook 与独立的 before-Pack 排除 Hook 的参考实现。它不注册地图项右键菜单命令。

Official Locale Tools 包含在官方插件安装内容中。Windows MSI 与 EXE 默认安装官方插件，macOS 用户需运行 DMG 中的 **Install Official Plugins**。从仓库运行时，会通过检出目录的 `plugins.json` 加载上述源码，并优先于用户目录中的副本。便携包则需要显式安装到 `~/Ludork`。使用前请按[安装与管理插件](<../安装与管理插件.md>)完成安装。

## 注册

公开入口类型加载插件自己的 `locale.json`，然后注册：

- 一条排序值为 900 的 **数据库** 菜单命令，用于打开 `Data/Locale/Locale.xlsx`
- 一个 `LocaleTextHintProvider`
- 通过 `RegisterBeforeExportHook` 注册的 `LocaleExportHook`
- 只排除创作工作簿的 before-Pack Hook

Export Hook 把工作簿声明为输入，把语言 Lua 文件声明为输出，用于判断是否需要重新导出；每次尝试导出后都会刷新自身缓存与编辑器文本提示。

工作簿编写、生成文件和打包规则见[游戏本地化工作流](<游戏本地化工作流.md>)。

## 菜单行为

菜单处理函数要求已有打开的项目，找到工作簿后调用操作系统打开。已处理的警告或错误通过 `IPluginUserInterface` 报告。

## 插件语言与游戏语言

`Plugins/OfficialLocaleTools/locale.json` 使用 `EditorLanguage` 本地化插件自己的菜单与诊断信息。它不是游戏的语言表。

开发插件时可参考它的组织方式：入口类负责注册，专用类执行具体操作，错误通过 `PluginResult` 返回，不依赖编辑器实现类型。

## 限制

该插件负责导出游戏文本；插件界面与游戏文本使用独立的语言资源。

## 相关页面

- [游戏本地化工作流](<游戏本地化工作流.md>)
- [注册点与 Hook 参考](<../注册点与 Hook 参考.md>)
