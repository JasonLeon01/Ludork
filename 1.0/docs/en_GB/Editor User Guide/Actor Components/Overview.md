# Actor Components

## Purpose

Components add optional presentation or child-Actor behaviour to an Actor. Configure class defaults in a Blueprint and override them for individual map instances when needed.

## Applicability

| Component | Field | Available on | Use |
|---|---|---|---|
| [Billboard](<Billboard.md>) | `billboardComp` | `Engine.Actor` and derived classes | Images and text above the Actor |
| [Light](<Light.md>) | `lightComp` | `Engine.Actor` and derived classes | A light that follows the Actor |
| [Emitter](<Emitter.md>) | `emitterComp` | `Engine.Actor` and derived classes | A Common Particle effect attached to the Actor |
| [ChildActor](<ChildActor.md>) | `childActorComp` | Default Gameplay `Source.MapActors.Enemy` and derived classes | A child Actor, used for the enemy damage readout |

The component list comes from the selected class's metadata and inheritance chain. `childActorComp` is declared by Enemy, not by the native Actor base; adding a component does not add a new field to an arbitrary class.

## Configuration

1. Open an Actor Blueprint to edit shared defaults, or select a placed Actor and open **Actor Info** to edit that instance.
2. In **Components**, use **+** to add an available optional component. Double-click an existing component to edit its fields. Follow the corresponding component page for its configuration.
3. Save the Blueprint or map document you edited.

A child Blueprint inherits an omitted value and uses its explicit value in preference to an ancestor's default. Optional components remain available even if no ancestor supplies a default; an unset component without a default is omitted from a new Blueprint's JSON. A map instance override takes precedence over its Blueprint defaults. In **Actor Info**, reset an overridden field to inherit again, or use **Reset All** to remove all class-field overrides for that instance.

## References

- [Blueprints, Common Functions and Actors](<../Blueprints Common Functions and Actors.md>)
- [Engine Actor and Component APIs](<../../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Gameplay Types.md#component>)
- [Default Gameplay: Actors, Enemies, Items and Equipment](<../../Lua and Blueprint Scripting/Default Gameplay/Actors Enemies Items and Equipment.md>)
