# Game Script Mixins

The Game project provides the following mixin examples. Each requires the parent capabilities below, its matching metadata and the relevant project configuration.

| Script | Parent capability | Behaviour |
|---|---|---|
| `Consumables/Bottle.lua` | Collidable Actor | Detects the Player, restores HP, plays a configured sound and destroys the pickup. |
| `Consumables/Gem.lua` | Collidable Actor | Adds a value to a direct Player field or matching component field. |
| `Doors/Door.lua` | `DoorBase` | Opens on collision and guards against repeated opening. |
| `Doors/KeyDoor.lua` | Script Mixin door Blueprint | Requires and consumes a configured General Data item before continuing the parent collision method. |
| `Doors/ConditionDoor.lua` | `DoorBase` | Enables polling only for a configured game variable and opens when it reaches the threshold. |

## Related pages

- [Script Mixin Authoring Workflow](<Authoring Workflow.md>)
- [Consumables](<../Default Gameplay/Recipes/Consumables.md>)
- [Doors](<../Default Gameplay/Recipes/Doors.md>)
