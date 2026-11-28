# Official General Data Blueprint Export

Official General Data Blueprint Export (`Plugins/OfficialGeneralDataBlueprintExport`) creates Actor Blueprints for [General Data members](<../../Editor User Guide/General Data and Text Config.md#type-structure>) that do not yet have one. Open **Plug-ins → Export General Data to Blueprints…** in a project; the Game project declares `Enemy`, `Equip`, `Item` and `Player` for export.

Tick the types you want to export and set each save folder under `Data/Blueprints` by typing a project-relative path or choosing **Browse…**. Folders default to `Data/Blueprints/<type>s` (for example, `Data/Blueprints/Enemys`, which you can change to `Data/Blueprints/Enemies`).

Select **Export** to create `BP_<member ID>.json` for each unrepresented member. Each generated Blueprint inherits the declared Lua parent class, binds its member field to the member ID and leaves `texturePath` empty so you can assign artwork in the Blueprint editor; icons, translated display names and other member values remain in General Data.

Before writing, the plug-in scans existing Blueprints across `Data` — including child Blueprints and current edits in the project window. Any Blueprint derived from the declared parent whose effective member field equals a member ID already covers that member (for example, a hand-authored `Slime` Blueprint in another folder), so repeated exports skip it. If a target filename is invalid, already occupied or collides with another member in the same run, the plug-in lists the conflict and still creates the remaining Blueprints.

## Declare an exportable type

Add a standalone single-line comment containing `@general-data-blueprint` and a JSON object to the Lua class implementation under `Scripts` (for example, `Scripts/Source/MapActors/Enemy.lua`):

```lua
-- @general-data-blueprint {"type":"Enemy","parent":"Source.MapActors.Enemy","memberField":"ID"}
```

- **`type`:** Exact General Data type key, declared at most once per project.
- **`parent`:** Runtime class reference the new Blueprint inherits, resolving to a class derived from `Engine.Actor`. Follow the [Lua parent reference rules](<../../Lua and Blueprint Scripting/Blueprint Scripting/Metadata Schema and Decorators.md#inheritance>) for modules that return a class directly or expose a named class.
- **`memberField`:** Editable Blueprint field that receives the member ID. Its schema must be `string` or the enum for that General Data type; `texturePath` is reserved for the Actor's texture.

The scan skips `Scripts/stub`, `.d.lua` and `_meta.lua`. Undeclared types (such as `State` in the Game project) do not appear in the window; invalid or duplicate declarations are reported as diagnostics.
