# Troubleshooting

Use this guide to diagnose common project, Blueprint, launch, asset, localisation and packaging failures in Default Gameplay projects. For native C++ crashes, binding-generation failures or plug-in load errors, consult the corresponding Native C++ Development or Plug-in Development pages.

## The project will not open

Validate JSON syntax in `Main.proj` and every loaded file under `Data`. Confirm that all paths stay within the project root, and restore any missing project directories rather than opening a partial package.

## A Blueprint will not save

Save errors point to invalid parents, inheritance cycles, nodes, links, pins or defaults. Script Mixin Blueprints additionally require a valid relative `scriptPath`, a script returning a plain table and a matching `_meta.lua` file; see [Script Mixin Authoring Workflow](<../Script Mixins/Authoring Workflow.md>).

## The game will not start

Inspect the Console starting from the first reported error:

- **Build and export order**: **Play** checks **Construct** (when required) before **Export**. Export validation failures, plug-in errors and failing before-Run hooks all block launch.
- **Scripts and modules**: Verify generated UI and locale files, native modules, the [startup script](<../Lua Runtime and Modules.md#entry-and-module-order>), Core module load order and runtime protocol diagnostics.
- **Material cache failures**: If `SceneInit` halts during material preparation, inspect its first generation or cache error. The runtime cache lives under the game's user data directory; see [Runtime generation](<../../Editor User Guide/Materials.md#runtime-generation>).

## A node is missing

Verify that the metadata file is pure data, returns `_METADATA`, declares the member with `type = "function"` or `type = "event"`, and lives outside `Scripts/Mixins`. For C++ bindings, rebuild so bindgen regenerates metadata.

## An asset or data reference is broken

Use the reference tree to find the invalid key or path. See [Assets](<../../Editor User Guide/Project Settings and Asset Layout.md#assets>) for rename and path rules, and [Type structure](<../../Editor User Guide/General Data and Text Config.md#type-structure>) for typed General Data references.

## Localised text is missing

Confirm that language-file IDs, the active language and `LOC` format arguments match the project text. See [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md>) for workbook and export configuration.

## Packaged behaviour differs

- **Runtime environment and paths**: Verify packaged assets, scripts, native modules, FFmpeg variant and the [writable game configuration location](<../../Editor User Guide/Project Settings and Asset Layout.md#game-configuration>). Never infer the platform from path separators; read the injected `PLATFORM` global instead.
- **Android packaging**: Requires a C++ Source project, a local Android Studio installation and its required tools. Packaging errors list missing prerequisites; see [Run, Debug and Package](<../../Editor User Guide/Run Debug and Package.md>) for expected paths and versions.
- **Android signing**: Confirm that the JKS or PKCS12 keystore path is absolute and readable, the key alias exists, and both passwords are correct. Fix stale saved values in the dialog and re-enter unsaved passwords. Failed runs publish neither unsigned intermediates nor partial APKs. Reuse the same signing key for all updates to an installed app.
