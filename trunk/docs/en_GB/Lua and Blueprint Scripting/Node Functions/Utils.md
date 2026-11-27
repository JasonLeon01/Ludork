# GlobalFunctions.Utils

`GlobalFunctions.Utils` provides general-purpose Blueprint nodes for flow control, local and game variables, party management, attributes, animations, the event bus and common functions.

**Metadata source:** `Scripts/GlobalFunctions/Utils_meta.lua`

`RunCommonFunction` shares the caller's local context and graph owner, including nested calls. `SELF` returns that owner, which is also the default for unconnected instance-method receivers inside the Common Function.

`SUPER(obj, params)` calls the parent implementation of the current Blueprint event. See [Parent event calls](<../Blueprint Scripting/Execution Flow Events and Variables.md#parent-event-calls>) for inheritance, parameter-array and `nil` fallback rules.

## Utils

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `IF` | `condition: bool = false` | `int` | Execution branches: `TRUE` (`0`), `FALSE` (`1`). |
| `SetLocalValue` | `valueName: string`, `value: any` | — | Sets a node-local variable in the current graph context. |
| `GetLocalValue` | `valueName: string`, `default: any` | `value: any` | Pure. Reads a node-local variable, returning `default` if unset. |
| `GetLocalValueRef` | `valueName: string`, `default: any` | `value: any` | Pure. Returns a mutable reference to a node-local variable. |
| `SetGameVariable` | `valueName: string`, `value: any` | — | `InstVar` selector on `valueName`; `InstVarValue` adapts `value` to `valueName`'s type. |
| `GetGameVariable` | `valueName: string`, `default: any` | `value: any` | Pure. `InstVar` selector on `valueName`; `InstVarValue` adapts `default` to `valueName`'s type. |
| `GetGameVariableRef` | `valueName: string`, `default: any` | `value: any` | Pure. Returns a mutable reference to a game variable. `InstVar` on `valueName`; `InstVarValue` binds `default` to `valueName`. |
| `AddPlayerByClass` | `playerClass: string = ""`, `mapPath: string = ""`, `position: sf.Vector2i` | — | Blueprint class picker on `playerClass` (`BlueprintClassVars`); `Transfer` links `position` to `mapPath`. |
| `SetPlayerByClass` | `playerClass: string = ""` | — | Blueprint class picker on `playerClass` (`BlueprintClassVars`). Switches the primary Player. |
| `RemovePlayerByClass` | `playerClass: string = ""` | — | Blueprint class picker on `playerClass` (`BlueprintClassVars`). Removes a party member. |
| `AddAnim` | `animName: string`, `position: sf.Vector2f = sf.Vector2f(0, 0)`, `rotation: float = 0`, `scale: sf.Vector2f = sf.Vector2f(1, 1)` | — | `Enums.Animation` selector. Spawns an animation at `position`. |
| `AddAnimOn` | `animName: string`, `actorTag: string`, `rotation: float = 0`, `scale: sf.Vector2f = sf.Vector2f(1, 1)` | — | `Enums.Animation` selector. Spawns an animation centred on the Actor matching `actorTag`. |
| `GetAnimLength` | `animName: string` | `value: float` | Pure. `Enums.Animation` selector. Returns the animation duration. |
| `GetAnimVisualLength` | `animName: string` | `value: float` | Pure. `Enums.Animation` selector. Returns the animation visual duration. |
| `SUPER` | `obj: any`, `params: any[] = {}` | `value: bool` | Invokes the parent Blueprint event implementation on `obj`. |
| `SELF` | — | `value: any` | Pure. Returns the current graph owner. |
| `GetAttrRef` | `obj: any`, `attrName: string` | `value: any` | Pure. Returns a mutable reference to a component field or object attribute. |
| `GetAttr` | `obj: any`, `attrName: string` | `value: any` | Pure. Reads a component field or object attribute. |
| `SetAttr` | `obj: any`, `attrName: string`, `value: any` | — | Writes a component field or object attribute. |
| `GetScene` | — | `value: GlobalCore.SceneBase` | Pure. Returns the active Scene from `SceneManager`. |
| `IsValidValue` | `value: any` | `value: bool` | Pure. Returns `true` when `value` is not `nil`. |
| `ToShortNumber` | `value: any = 0` | `value: any` | Pure. Formats a number using compact short notation. |
| `RunCommonFunction` | `commonFunctionName: string = ""` | `any` | Common Function picker on `commonFunctionName` (`CommonFunctionVars`). |
| `RegisterEventBus` | `key: string`, `obj: any`, `functionName: string` | — | Registers an object method on the event bus under `key`. |
| `RegisterEventBusEvent` | `key: string`, `obj: any`, `eventName: string` | — | Registers a Blueprint event on the event bus under `key`. |
| `UnregisterEventBus` | `key: string` | `value: bool` | Unregisters all handlers for `key` from the event bus. |
| `UnregisterEventBusEvent` | `key: string`, `obj: any` | `value: bool` | Unregisters `obj`'s event handler for `key` from the event bus. |
| `TriggerEventBus` | `key: string`, `kwargs: any = {}` | — | Triggers all event-bus handlers registered under `key`. |
| `TriggerBlueprintEvent` | `obj: any`, `eventName: string` | — | Triggers the named Blueprint event on `obj`. |
| `BackToTitle` | — | — | Returns to the title Scene. |
| `Print` | `message: any = ""` | — | Logs `message` to the console output. |
| `EXEC` | `script: string = ""` | — | Executes a Lua script string. |
| `GetSelfAttr` | `attrName: string` | `value: any` | Pure. Reads a component field or attribute on the current graph owner. |
| `SetSelfAttr` | `attrName: string`, `value: any` | — | Writes a component field or attribute on the current graph owner. |
| `IfPlayerOverlaps` | — | `value: bool` | Pure. Returns whether the Player overlaps the current Actor. |
| `IfGameVar` | `varName: string = ""`, `op: string = "=="`, `value: any` | `bool` | Execution branches: `TRUE` (`true`), `FALSE` (`false`). `op` drop-down: `==`, `!=`, `<`, `<=`, `>`, `>=`. `InstVar` on `varName`; `InstVarValue` binds `value` to `varName`. |

`AddPlayerByClass` stores the new member's map and cell without changing the primary player. `SetPlayerByClass` moves that class to `playerKeys[1]`, rebinds Scene UI and loads the stored map.

`GetAttr` and a read through `GetAttrRef` both return the component field value when it is non-nil; otherwise they read the object's attribute. Both preserve `false`, `0` and empty strings, and both raise an error when no non-nil value exists. Reading an Actor's `tag` returns its string value, which can connect directly to the `refActorTag` input of dialogue nodes.
