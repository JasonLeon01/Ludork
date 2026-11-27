# Source.Components.ChildActorComponent

`ChildActorComponent` declares a class name and a relative position for spawning and attaching a child Actor. The editable `childActorComp` field is declared on `Source.MapActors.Enemy` and inherited by its descendants; see [ChildActor Component](<../../Editor User Guide/Actor Components/ChildActor.md>) for editor configuration.

**Metadata source:** `Scripts/Source/Components/ChildActorComponent_meta.lua`

## ChildActorComponent

**Inherits:** `Engine.Component`

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `className` | `string` | `""` | Child Actor class name to instantiate. |
| `relativePosition` | `sf.Vector2f` | `sf.Vector2f(0, 0)` | Position offset relative to the owning Actor. |
