# Ludork 文档

本指南适用于 Ludork 1.0，可在[在线文档](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=zh_CN)阅读。下载与项目信息见 [Ludork 官网](https://jasonleon01.github.io/Ludork/?lang=zh_CN)，[第三方声明](https://jasonleon01.github.io/Ludork/notices/?version=1.0&lang=zh_CN)也可从官网页脚打开。章节标签右侧可切换文档语言和版本。

Ludork 集可视化编辑器、蓝图、Lua 脚本和可扩展的 C++20 运行时于一体，用于制作以地图为基础的 2D 游戏。可在编辑器中配置项目，制作地图、Actor、数据、动画和界面，再运行和打包游戏。

![打开 Game 项目地图的 Ludork 编辑器工作区](../../images/overview/editor-workspace.png)

*主工作区把地图画布、编辑模式、项目数据和运行工具组合在一起。*

## 选择阅读路线

| 如果你需要…… | 从这里开始 |
|---|---|
| 制作可玩的游戏项目 | [创建第一个项目](<快速入门/创建第一个项目.md>) |
| 编辑地图、资产与项目数据 | [启动页、工作区与快捷键](<编辑器用户指南/启动页、工作区与快捷键.md>) |
| 配置 Actor 的头顶提示、光源、粒子与子 Actor | [Actor 组件](<编辑器用户指南/Actor 组件/概述.md>) |
| 查 Palette 里每个控件的用途 | [Palette 控件](<编辑器用户指南/Palette 控件.md>) |
| 为 Actor 和 UI 制作 GPU 粒子效果 | [通用粒子](<编辑器用户指南/通用粒子.md>) |
| 为视频添加定时字幕 | [视频字幕](<编辑器用户指南/视频字幕.md>) |
| 学习 Lua | [Lua 基础语法](<快速入门/Lua 基础语法.md>) |
| 用 Lua 或蓝图编写玩法 | [Lua 运行时与模块](<Lua 与蓝图脚本/Lua 运行时与模块.md>) |
| 新增或修改原生运行时 API | [构建与模块布局](<C++ 原生开发/构建与模块布局.md>) |
| 部署可选的远端账号存储 | [简易服务器](<简易服务器/部署与编辑器设置.md>) |
| 安装或编写可信的编辑器扩展 | [安装与管理插件](<插件开发与安装/安装与管理插件.md>) |
| 查找无引用的项目资源 | [Official Resource Cleanup](<插件开发与安装/Official 插件/Resource Cleanup.md>) |

## 支持的平台

### 编辑器

| 平台 | 最低支持版本 |
|---|---|
| Windows | Windows 10，x64 |
| macOS | Apple Silicon，macOS 13.3 |

### 游戏

| 平台 | 最低支持版本 | 说明 |
|---|---|---|
| Windows | Windows 10，x64 | |
| macOS | Apple Silicon，macOS 13.3 | |
| iOS | iOS 16.3，arm64 | 需要 C++ Source 项目。 |
| HarmonyOS | HarmonyOS 6.0.2 / API 22，arm64-v8a | 支持 Mobile 和 2in1，需要 C++ Source 项目。 |
| Android | Android 7.0 / API 24，arm64-v8a | 需要 C++ Source 项目。 |

移动端发行包在 Apple Silicon macOS 上生成。Standalone 项目使用与编辑器所在平台相同的预构建运行时。平台工具和签名选项见[运行、调试与打包](<编辑器用户指南/运行、调试与打包.md>)。

## 文档约定

- 除非页面另有说明，路径均相对于项目根目录。
- 菜单命令与字段标签按编辑器界面中的实际写法书写。
- 底层平台区分大小写时，代码、文件名与 API 标识符同样区分大小写。

## 软件与内容许可

Ludork 软件、第三方组件和 Game 项目资产各有自己的许可条款。重新分发编辑器或使用 Game 资产发布游戏时，需要分别遵守这些条款。插件会以编辑器的权限运行，请只安装来源可信的插件。
