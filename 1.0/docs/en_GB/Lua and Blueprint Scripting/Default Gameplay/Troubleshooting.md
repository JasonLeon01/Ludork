# Troubleshooting

## The project will not open

Check `Main.proj` and every loaded file beneath `Data` for valid JSON. Paths must remain inside the project. Restore required directories rather than pointing the editor at a partial package.

## A Blueprint will not save

A save error identifies invalid parents, inheritance cycles, nodes, links, pins or defaults. Script Mixins also require a valid relative path, returned table and same-name metadata file. See [Script Mixin Authoring Workflow](<../Script Mixins/Authoring Workflow.md>) for those rules.

## The game will not start

Read the Console from the first failure. Complete Construct when required, then Export; Play checks these in that order. Export validation or plug-in failures block launch, as do before-Run hook failures. Check the generated UI and locale files, `Data/MaterialGenerated`, native modules, the `Main.ini` entry script, Core module order and runtime protocol diagnostics.

## A node is missing

Confirm that its metadata file is pure data, returns `_METADATA`, names the member with `type = "function"` or `"event"`, and is outside `Scripts/Mixins`. For C++ declarations, rebuild so bindgen regenerates metadata.

## An asset or data reference is broken

Open the reference tree, find the invalid stored key or path, and update it manually. Renaming and deleting do not promise automatic reference repair.

## Localised text is missing

Language-file IDs, the active language and `LOC` formatting must match the project text. See [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md>) for workbook and export settings.

## Packaged behaviour differs

Check the package's assets, scripts, native modules, FFmpeg variant, writable `Main.ini` location and platform-specific path handling. Never infer the platform from path separators. Use the injected `PLATFORM` value.

Android packaging requires a C++ Source project, a local Android Studio installation and its supporting tools. Packaging errors list missing requirements; see [Run, Debug and Package](<../../Editor User Guide/Run Debug and Package.md>) for installation paths and versions.

If Android signing fails, check that the JKS or PKCS12 path is absolute and readable, that the alias exists, and that both passwords are correct. Correct stale saved values in the dialog, and enter unsaved values again. A failed run publishes neither its unsigned intermediate nor a partial signed APK. Use the same key for every later version that must update the installed application.

## Limitations

Native crashes, binding-generation failures and plug-in load failures require the specialised diagnostics in their respective development sections.

## Related pages

- [Run, Debug and Package](<../../Editor User Guide/Run Debug and Package.md>)
- [C++ Binding Troubleshooting](<../../Native C++ Development/C++ Binding Troubleshooting.md>)
- [Distribution](<../../Plug-in Development and Installation/Distribution.md>)
