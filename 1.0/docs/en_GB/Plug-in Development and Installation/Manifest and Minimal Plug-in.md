# Manifest and Minimal Plug-in

## Goal

Start with a generated C# 13 template, or write a complete source plug-in that adds one Help menu command and can be imported by Ludork 1.0.0.

## Prerequisites

- .NET 9 SDK if using an IDE project for diagnostics and local compilation.
- Access to `Ludork.Plugin.Abstractions` from this repository or the editor development kit.
- A plug-in ID unique in its effective scope and a CLR namespace.

## Generated template

**Plugins → Manage Plugins → New Plugin...** creates this source directory in the selected global or current-project scope:

```text
<plugin-id>/
├── plugin.json
├── Plugin.cs
└── PluginWindow.cs   (With a Window only)
```

The generated manifest uses version `1.0.0`, the current editor version as `minimumEditorVersion`, and `Ludork.UserPlugins.Plugin` as `entryType`. Each plug-in compiles into its own assembly, so generated templates can share this CLR type name. The entry class is:

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

**With a Window**, the default, also creates an independent `PluginWindow : Window` class using the host theme. **Logic Only** creates only the manifest and entry class. Neither template registers commands; the window is not opened automatically. No XAML or IDE project is generated. Add your logic and registrations, then restart to load the edited source.

## Hand-written command example

The following complete example adds one Help menu command. Its optional project file supports IDE diagnostics.

### Source directory

```text
MyPlugin/
├── plugin.json
├── MyPlugin.cs
└── MyPlugin.csproj
```

There is one source-directory plug-in format. Sources are compiled in relative-path order with C# 13, nullable enabled, unsafe allowed and release optimisation. The host does not restore NuGet packages. It does not load a prebuilt entry assembly either.

### Write `plugin.json`

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

| Field | Contract |
|---|---|
| `schemaVersion` | Manifest schema; the value must equal `1` |
| `id` | Identifier containing only letters, digits, dots, hyphens or underscores |
| `name` | Non-empty display name |
| `version` | Version accepted by .NET `Version` parsing |
| `minimumEditorVersion` | Minimum editor version parsed as a .NET `Version` |
| `entryType` | Namespace-qualified CLR type name, matched case-sensitively |

JSON property names are case-sensitive.

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

The entry type must be public, non-abstract, implement `IEditorPlugin` and have a public parameterless constructor. Plug-in IDs and command IDs must be unique within the effective global-plus-current-project set. Independent project scopes may reuse IDs.

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

Adjust the reference to match the source layout. Official plug-ins in this repository live under `Plugins`, so this relative path reaches `Editor/Ludork.Plugin.Abstractions`. The project is for IDE analysis only. Every source needed at runtime must remain in the imported directory and use host-provided dependencies.

### Import and restart

Choose **Plugins → Import Plugin**, select the target scope and `MyPlugin` and accept the full-trust confirmation, then restart Ludork.

## Related pages

- [Installing and Managing Plug-ins](<Installing and Managing Plug-ins.md>)
- [Avalonia UI, Secrets and Localisation](<Avalonia UI Secrets and Localisation.md>)
- [Testing and Distribution](<Testing and Distribution.md>)
