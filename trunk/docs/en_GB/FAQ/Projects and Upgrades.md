# Projects and Upgrades

## Which project template should I choose?

Choose **Standalone** to build gameplay with Lua, Blueprint, data and assets using the prebuilt runtime without a native compiler. Choose **C++ Source** to modify native services or bindings, or to package for iOS, HarmonyOS, Android or Web. Video playback is an independent template option. See [Template reference](<../Getting Started/Project Templates.md#template-reference>).

## Why is a project template unavailable, and can I create projects offline?

Release editors download templates matching their complete version, including the patch number; if that release or template has not been published, project creation reports it as unavailable. A completed, valid template cache can be reused offline. Development editors use bundled templates and report missing files without downloading replacements. See [Obtaining templates](<../Getting Started/Project Templates.md#obtaining-templates>).

## Will updating Ludork automatically update an existing project?

Updates to the Game project and default templates do not automatically modify projects already created from them; apply any desired changes to each project manually. `Main.proj.version` records the engine compatibility version (`v1.0` for this edition), which is separate from the game's release version and save format. See [Template variants](<../Lua and Blueprint Scripting/Default Gameplay/Default Template Overview.md#template-variants>) and [Main.proj](<../Editor User Guide/Project Settings and Asset Layout.md#mainproj>).
