# Running Debugging and Publishing

## Which platforms can I develop on and publish to?

The editor runs on Windows x64 and Apple Silicon macOS. Games can target those platforms as well as iOS, HarmonyOS, Android and Web. Mobile and Web packaging require a **C++ Source** project; mobile packages are produced on Apple Silicon macOS, and **Standalone** uses the prebuilt runtime for the editor's host platform. See [Supported platforms](<../Ludork Documentation.md#supported-platforms>) for minimum versions and [Packaging](<../Editor User Guide/Run Debug and Package.md#packaging>) for required tools.

## Why is Play unavailable, and when do I use Construct or Export?

Complete a successful **Export** before the first **Play**. **C++ Source** projects also require a successful Debug build through **Construct** or `tools/build_cpp`. **Construct** compiles native code, **Export** generates UI and locale runtime files and `SceneInit` prepares materials; Lua, UI JSON, game data and localisation edits do not require a native build. Check the first reported error if preparation fails. See [Construct, Export and Play](<../Editor User Guide/Run Debug and Package.md#construct-export-and-play>).

## Does Live Debug save changes back to my project?

**Live Debug** edits current runtime instances without modifying project documents or providing Undo/Redo. Enable it before **Play** and use **Individual Window**. Saving the game preserves gameplay state modified by live edits. See [Live Debug](<../Editor User Guide/Run Debug and Package.md#live-debug>) for available edits and instance lifetime.

## Why is Confirm disabled in Pack Project?

Hover over the disabled button to view the reason. The entry script must declare a valid static `APP_NAME` (initially supplied through [New Project](<../Getting Started/Project Templates.md#template-reference>)). See [Packaging](<../Editor User Guide/Run Debug and Package.md#packaging>) for declaration rules and other packaging requirements.
