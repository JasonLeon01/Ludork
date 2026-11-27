# Manifest and Minimal Plug-in

Generate a C# 13 plug-in template directly from the editor, or author a minimal source plug-in from scratch that adds one Help menu command for Ludork 1.0.0 and newer.

## Requirements

- .NET 9 SDK (optional; required only if you use an IDE `.csproj` file for diagnostics and local builds).
- Access to `Ludork.Plugin.Abstractions` from the Ludork repository or editor development kit.
- A plug-in ID unique within its effective scope and a matching CLR namespace.

## Generated template

Open **Plug-ins → Manage Plug-ins** and select **New Plug-in...** to create a ready-to-edit plug-in directory in the chosen global or current-project scope:

```text
<plugin-id>/
├── plugin.json
├── Plugin.cs
└── PluginWindow.cs   (With a Window only)
```

The generated manifest sets `version` to `1.0.0`, `minimumEditorVersion` to the current editor version and `entryType` to `Ludork.UserPlugins.Plugin`. Because each plug-in compiles into its own assembly, generated templates can share that CLR type name:

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

**With a Window** (default) adds an independent `PluginWindow : Window` styled with the host theme; **Logic Only** generates only `plugin.json` and `Plugin.cs`. Neither template registers commands, opens a window automatically, or generates XAML or a `.csproj` file. Add your registrations and logic, then restart Ludork to load the updated source.

## Hand-written command example

The example below creates a complete source plug-in that adds one **Help** menu command, with an optional `.csproj` for IDE analysis.

### Source directory

```text
MyPlugin/
├── plugin.json
├── MyPlugin.cs
└── MyPlugin.csproj
```

Ludork supports one source-directory plug-in format. The editor compiles `.cs` files in relative-path order using C# 13 with nullable reference types enabled, `unsafe` allowed and Release optimisation. The host never restores NuGet packages or loads a prebuilt entry assembly.

### Write `plugin.json`

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

| Field | Contract |
|---|---|
| `schemaVersion` | Manifest schema version; must equal `1` |
| `id` | Identifier containing only ASCII letters, digits, dots, hyphens or underscores |
| `name` | Non-empty display name |
| `version` | Version string accepted by .NET `Version` parsing |
| `minimumEditorVersion` | Minimum editor version parsed as a .NET `Version` |
| `entryType` | Namespace-qualified CLR type name, matched case-sensitively |

All JSON property names are case-sensitive.

### Add the entry type

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

The entry type must be `public`, non-abstract, implement `IEditorPlugin` and expose a public parameterless constructor. Plug-in IDs and command IDs must be unique within the effective global-plus-current-project set; separate project scopes may reuse IDs.

### Add an optional IDE project

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

Adjust the relative path to match your checkout layout (official plug-ins under `Plugins/` reach `Editor/Ludork.Plugin.Abstractions` with `..\..`). This project file is strictly for IDE diagnostics: every runtime `.cs` source must stay inside the imported plug-in directory and rely only on host-provided references.

### Import and restart

[Import the complete `MyPlugin` directory and restart Ludork](<Installing and Managing Plug-ins.md#import-a-third-party-plug-in>).
