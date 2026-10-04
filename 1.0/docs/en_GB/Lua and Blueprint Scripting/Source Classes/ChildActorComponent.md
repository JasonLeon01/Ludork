# Source.Components.ChildActorComponent

`ChildActorComponent` declares a class name and a relative position for a child Actor.

The editable `childActorComp` field is declared by Default Gameplay Enemy and inherited by its descendants. See [ChildActor Component](<../../Editor User Guide/Actor Components/ChildActor.md>) for editor configuration.

Metadata source: `Scripts/Source/Components/ChildActorComponent_meta.lua`

## ChildActorComponent

Direct metadata bases: `{ "Engine", "Component" }`

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `className` | `string` | `""` | — |
| `relativePosition` | `sf.Vector2f` | `{ [1] = 0, [2] = 0 }` | — |

### Functions and events

No Blueprint functions or events are declared.
