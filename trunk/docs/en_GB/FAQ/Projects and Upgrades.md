# Projects and Upgrades

## Which project template should I choose?

Choose **Standalone** to build gameplay with Lua, Blueprint, data and assets using the supplied runtime; no native compiler is needed. Choose **C++ Source** to change native services or bindings, or to package for iOS, HarmonyOS or Android. Video playback is a separate template option. See [Template reference](<../Getting Started/Project Templates.md#template-reference>).

## Why is a project template unavailable, and can I create projects offline?

Release editors download templates matching their complete version, including the patch number. If that release or template has not been published, creation reports that it is unavailable. A completed, valid template cache can be reused offline. Development editors use bundled templates and report missing files without downloading a replacement. See [Obtaining templates](<../Getting Started/Project Templates.md#obtaining-templates>).

## Will updating Ludork automatically update an existing project?

Changes to the Game project and default templates do not update projects already created from them; apply the changes to each project yourself. `Main.proj.version` records the engine compatibility version, which is `v1.0` for this edition. It is separate from the game's release version and save format. See [Template variants](<../Lua and Blueprint Scripting/Default Gameplay/Default Template Overview.md#template-variants>) and [Main.proj](<../Editor User Guide/Project Settings and Asset Layout.md#mainproj>).
