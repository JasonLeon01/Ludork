# Running Debugging and Publishing

## Which platforms can I develop on and publish to?

The editor runs on Windows x64 and Apple Silicon macOS. Games can target those platforms as well as iOS, HarmonyOS and Android. Mobile packaging requires a C++ Source project and Apple Silicon macOS; Standalone uses the runtime for the editor's host platform. Check [Supported platforms](<../Ludork Documentation.md#supported-platforms>) for minimum versions and [Packaging](<../Editor User Guide/Run Debug and Package.md#packaging>) for the required tools.

## Why is Play unavailable, and when do I use Construct or Export?

Complete a successful **Export** before the first Play. C++ Source projects also need a successful Debug build through **Construct** or `tools/build_cpp`. Construct compiles native code; Export generates UI, locale and material runtime files. Lua, UI JSON, game data and localisation changes do not require a native build. Read the first reported error if preparation fails. See [Construct, Export and Play](<../Editor User Guide/Run Debug and Package.md#construct-export-and-play>).

## Does Live Debug save changes back to my project?

Live Debug edits the current runtime instances without writing project documents or providing Undo/Redo. Enable it before Play and use **Individual Window**. A game save can still preserve gameplay state affected by live edits. See [Live Debug](<../Editor User Guide/Run Debug and Package.md#live-debug>) for the available edits and instance lifetime.

## Why is Confirm disabled in Pack Project?

Hover over the disabled button to read the reason. `Scripts/Entry.lua` must define exactly one top-level `local APP_NAME` as a non-empty static string other than `LudorkSample`. `APP_NAME` supplies the application's stable identity; the System title supplies its display name. See [Packaging](<../Editor User Guide/Run Debug and Package.md#packaging>).
