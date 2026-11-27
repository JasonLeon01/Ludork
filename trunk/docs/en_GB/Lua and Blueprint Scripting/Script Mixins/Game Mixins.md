# Game Script Mixins

Default Gameplay includes the Script Mixin examples below. Each requires the listed parent capability, its companion `_meta.lua` file and the referenced project data or configuration.

| Script | Parent capability | Behaviour |
|---|---|---|
| `Consumables/Bottle.lua` | Collidable Actor | Detects the Player, restores `HP`, plays a configured sound and destroys the pickup. |
| `Consumables/Gem.lua` | Collidable Actor | Adds a value to a direct Player attribute or matching component field. |
| `Doors/Door.lua` | `DoorBase` | Opens on collision and guards against repeated opening. |
| `Doors/KeyDoor.lua` | Script Mixin door Blueprint | Requires and consumes a configured General Data item before calling the parent collision method. |
| `Doors/ConditionDoor.lua` | `DoorBase` | Enables polling only when a game variable is configured, and opens the door once it reaches the threshold. |

For end-to-end setup walkthroughs, see [Consumables](<../Default Gameplay/Recipes/Consumables.md>) and [Doors](<../Default Gameplay/Recipes/Doors.md>).
