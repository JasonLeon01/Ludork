# Manifest 与最小插件

你可以直接在编辑器中生成 C# 13 插件模板，也可以从零手写一个适用于 Ludork 1.0.0 及以上版本、向**帮助**菜单添加命令的最小源码插件。

## 环境要求

- .NET 9 SDK（可选；仅在使用 `.csproj` 文件进行 IDE 诊断与本地编译时需要）。
- 可从 Ludork 仓库或编辑器开发包访问 `Ludork.Plugin.Abstractions`。
- 在有效作用域内唯一的插件 ID 以及对应的 CLR 命名空间。

## 自动生成模板

打开**插件 → 插件管理**并点击**新建插件...**，编辑器会在所选的全局或当前项目作用域下生成可直接编辑的插件目录：

```text
<plugin-id>/
├── plugin.json
├── Plugin.cs
└── PluginWindow.cs   （仅“带界面”模板）
```

生成的清单默认将 `version` 设为 `1.0.0`、`minimumEditorVersion` 设为当前编辑器版本、`entryType` 设为 `Ludork.UserPlugins.Plugin`。由于每个插件都会编译为独立程序集，不同生成模板可以共用该 CLR 类型名：

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

**带界面**模板（默认）会额外生成一个继承宿主主题的独立 `PluginWindow : Window` 类；**不带界面**模板仅生成 `plugin.json` 与 `Plugin.cs`。两种模板都不会自动注册命令、打开窗口，也不会生成 XAML 或 `.csproj` 文件。添加注册代码与业务逻辑后，重启 Ludork 即可加载更新后的源码。

## 手写命令示例

下面的完整示例创建一个向**帮助**菜单添加一条命令的源码插件，并附带可选的 `.csproj` 文件用于 IDE 分析。

### 源码目录

```text
MyPlugin/
├── plugin.json
├── MyPlugin.cs
└── MyPlugin.csproj
```

Ludork 仅支持单一的源码目录插件格式。编辑器按相对路径顺序使用 C# 13 编译全部 `.cs` 文件，默认启用可空引用类型、允许 `unsafe` 并开启 Release 优化。宿主不会还原 NuGet 包，也不会加载预编译的入口程序集。

### 编写 `plugin.json`

```json
{
  "schemaVersion": 1,
  "id": "Example.MyPlugin",
  "name": "My Plug-in",
  "version": "1.0.0",
  "minimumEditorVersion": "1.0.0",
  "entryType": "Example.MyPlugin.Plugin"
}
```

| 字段 | 契约 |
|---|---|
| `schemaVersion` | 清单 Schema 版本，必须等于 `1` |
| `id` | 仅含 ASCII 字母、数字、点、连字符或下划线的唯一标识符 |
| `name` | 非空显示名称 |
| `version` | 可被 .NET `Version` 解析的版本字符串 |
| `minimumEditorVersion` | 可被 .NET `Version` 解析的最低编辑器版本 |
| `entryType` | 带命名空间限定的 CLR 类型名，区分大小写匹配 |

所有 JSON 属性名均区分大小写。

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

入口类型必须为 `public`、非抽象、实现 `IEditorPlugin` 并提供公共无参构造函数。插件 ID 与命令 ID 必须在有效的“全局＋当前项目”集合内保持唯一；不同项目作用域之间允许复用相同 ID。

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

请按实际检出目录布局调整相对路径（仓库内位于 `Plugins/` 下的官方插件通过 `..\..` 引用 `Editor/Ludork.Plugin.Abstractions`）。该项目文件仅用于 IDE 诊断：所有运行时 `.cs` 源码都必须位于被引入的插件目录内，并仅依赖宿主提供的程序集引用。

### 引入并重启

完成后，请[引入完整的 `MyPlugin` 目录并重启 Ludork](<安装与管理插件.md#引入第三方插件>)。
