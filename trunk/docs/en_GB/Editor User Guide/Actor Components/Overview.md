# Actor Components

Attach optional presentation or child-Actor behaviour to an Actor by configuring components as class defaults in a Blueprint or overriding them on individual map instances.

| Component | Field | Available on | Purpose |
|---|---|---|---|
| [Billboard](<Billboard.md>) | `billboardComp` | `Engine.Actor` and derived classes | Stack of images and text above the Actor |
| [Light](<Light.md>) | `lightComp` | `Engine.Actor` and derived classes | Dynamic light following the Actor's transform |
| [Emitter](<Emitter.md>) | `emitterComp` | `Engine.Actor` and derived classes | Common Particle effect attached to the Actor |
| [ChildActor](<ChildActor.md>) | `childActorComp` | Default Gameplay `Source.MapActors.Enemy` and derived classes | Child Actor (used for the enemy damage readout) |

Available components come from the selected class's metadata and inheritance chain. For example, `childActorComp` is declared on `Enemy`, not native `Engine.Actor`; adding a component never introduces a new field onto an arbitrary class.

## Configuration

1. Open an Actor Blueprint to edit class defaults, or select a placed Actor and open **Actor Info** for instance overrides.
2. In **Components**, click **+** to add an available optional component, or double-click an existing component to edit its fields.
3. Save the edited Blueprint or map document.

Child Blueprints inherit omitted values and override ancestor defaults when explicitly configured; unset optional components without ancestor defaults are omitted from new Blueprint JSON. Map instance overrides take precedence over Blueprint defaults — use a field's reset button in **Actor Info** to inherit again, or **Reset All** to clear every class-field override on that instance.

**Related guides and APIs:** [Blueprints, Common Functions and Actors](<../Blueprints Common Functions and Actors.md>) · [Engine Actor and Component APIs](<../../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Gameplay Types.md#component>) · [Default Gameplay: Actors, Enemies, Items and Equipment](<../../Lua and Blueprint Scripting/Default Gameplay/Actors Enemies Items and Equipment.md>)
