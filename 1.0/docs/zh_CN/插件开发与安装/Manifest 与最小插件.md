# Manifest 与最小插件

## 目标

从生成的 C# 13 模板开始，或手写一个为帮助菜单添加命令、能被 Ludork 1.0.0 引入的完整源码插件。

## 前置条件

- 如需 IDE 项目诊断与本地编译，准备 .NET 9 SDK。
- 可以从本仓库或编辑器开发包访问 `Ludork.Plugin.Abstractions`。
- 在有效作用域内唯一的插件 ID，以及 CLR 命名空间。

## 自动生成模板

**插件 → 插件管理 → 新建插件...** 会在所选的全局或当前项目作用域下创建以下源码目录：

```text
<插件 ID>/
├── plugin.json
├── Plugin.cs
└── PluginWindow.cs   （仅带界面）
```

生成的 manifest 使用版本 `1.0.0`，`minimumEditorVersion` 为当前编辑器版本，`entryType` 为 `Ludork.UserPlugins.Plugin`。每个插件独立编译为程序集，因此不同模板可以使用相同的 CLR 类型名。入口类为：

```csharp
using Ludork.Plugin.Abstractions;

namespace Ludork.UserPlugins;

public sealed class Plugin : IEditorPlugin
{
    public void Register(IPluginRegistrar registrar)
    {
    }
}
```

默认的 **带界面** 模板还会生成独立的 `PluginWindow : Window` 类，并应用宿主主题。**不带界面** 只生成 manifest 与入口类。两种模板都不会注册命令，窗口也不会自动打开。不生成 XAML 或 IDE 项目。添加业务逻辑与注册代码后，重启以加载修改后的源码。

## 手写命令示例

下面的完整示例会添加一条帮助菜单命令，可选的项目文件用于 IDE 诊断。

### 源码目录

```text
MyPlugin/
├── plugin.json
├── MyPlugin.cs
└── MyPlugin.csproj
```

插件只有源码目录这一种格式。源码按相对路径顺序编译，采用 C# 13，启用 nullable，允许 unsafe，并使用 Release 优化。宿主不还原 NuGet 包，也不加载预构建的入口程序集。

### 编写 `plugin.json`

```json
{
  "schemaVersion": 1,
  "id": "Example.MyPlugin",
  "name": "My Plugin",
  "version": "1.0.0",
  "minimumEditorVersion": "1.0.0",
  "entryType": "Example.MyPlugin.Plugin"
}
```

| 字段 | 契约 |
|---|---|
| `schemaVersion` | manifest 使用的 schema；取值必须等于 `1` |
| `id` | 只含字母、数字、点、连字符或下划线的标识 |
| `name` | 非空显示名 |
| `version` | 能被 .NET `Version` 解析的版本 |
| `minimumEditorVersion` | 按 .NET `Version` 解析的最低编辑器版本 |
| `entryType` | 带命名空间限定的 CLR 类型名，区分大小写匹配 |

JSON 属性名区分大小写。

### 添加入口类型

```csharp
using Ludork.Plugin.Abstractions;
using System;
using System.Threading.Tasks;

namespace Example.MyPlugin;

public sealed class Plugin : IEditorPlugin
{
    public void Register(IPluginRegistrar registrar)
    {
        ArgumentNullException.ThrowIfNull(registrar);
        PluginMenuCommand command = new(
            "Example.MyPlugin.About",
            PluginMenuLocation.Help,
            500,
            "My Plug-in",
            ShowMessageAsync);
        registrar.RegisterMenuCommand(command);
    }

    private static async Task<PluginResult> ShowMessageAsync(
        PluginMenuContext context)
    {
        await context.UserInterface.ShowMessageAsync(
            "My Plug-in",
            "The plug-in is loaded.",
            PluginMessageKind.Information,
            context.CancellationToken);
        return PluginResult.Completed();
    }
}
```

入口类型必须公开、非抽象、实现 `IEditorPlugin`，并提供公开的无参构造函数。插件 ID 和命令 ID 必须在有效的“全局＋当前项目”集合内唯一；不同项目作用域可以使用相同 ID。

### 添加可选的 IDE 项目

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net9.0</TargetFramework>
    <LangVersion>13.0</LangVersion>
    <Nullable>enable</Nullable>
    <ImplicitUsings>disable</ImplicitUsings>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="..\..\Editor\Ludork.Plugin.Abstractions\Ludork.Plugin.Abstractions.csproj">
      <Private>false</Private>
    </ProjectReference>
  </ItemGroup>
</Project>
```

按实际的源码布局调整该引用。仓库里的官方插件位于 `Plugins`，因此这一相对路径指向 `Editor/Ludork.Plugin.Abstractions`。该项目只供 IDE 分析使用。运行时需要的所有源码都必须留在引入目录内，并使用宿主提供的依赖项。

### 引入并重启

选择 **插件 → 引入插件**，选择目标作用域和 `MyPlugin` 并接受完全信任确认，然后重启 Ludork。

## 相关页面

- [安装与管理插件](<安装与管理插件.md>)
- [Avalonia UI、密钥与本地化](<Avalonia UI、密钥与本地化.md>)
- [测试与分发](<测试与分发.md>)
