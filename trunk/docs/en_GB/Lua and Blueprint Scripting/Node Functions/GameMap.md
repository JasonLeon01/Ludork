# GlobalFunctions.GameMap

`GlobalFunctions.GameMap` provides pure Blueprint nodes for looking up Actors on the active map.

**Metadata source:** `Scripts/GlobalFunctions/GameMap_meta.lua`

## GameMap

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `GetActorByTag` | `tag: string` | `actor: Engine.Actor` | Pure. Returns the Actor matching `tag` on the active map. |
| `GetAllActors` | — | `actors: Engine.Actor[]` | Pure. Returns all Actors on the active map. |
