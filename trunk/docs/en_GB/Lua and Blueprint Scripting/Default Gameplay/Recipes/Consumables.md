# Consumables

Create collision-driven healing bottles and stat-boosting gems using Default Gameplay's Script Mixins. This workflow requires a Blueprint derived from `ConditionalActor` with collision enabled, a matching numeric field in the generated Player Attribute Set and a valid Audio configuration fallback.

## Configure a consumable Blueprint

1. Create a Blueprint derived from `Source.MapActors.ConditionalActor.ConditionalActor`, enable collision and enable **Script Mixin**.
2. Attach `Consumables/Bottle.lua` (healing) or `Consumables/Gem.lua` (attribute boost).
3. Configure `HP_plus` for a healing bottle, or `ATTR_key` and `plus` for a gem.
4. Choose a pickup sound in `getSE`, or leave it at its default to use `Audio.getSE` from the Audio configuration.

On collision, the shared pickup flow locates the Player, applies an Instant Add Effect (`Bottle` modifies `HP`; `Gem` modifies the `int` or `float` Player attribute named by `ATTR_key`), records the pickup as destroyed and destroys the Actor.

## Default template examples

- **`BP_Bottle_200.json`**: Derives directly from `Source.MapActors.ConditionalActor.ConditionalActor`, enables `scriptMixin`, attaches `Consumables/Bottle.lua`, enables collision, sets `HP_plus = 200` and assigns the potion texture.
- **`BP_Gem_1_1.json`**: Attaches `Consumables/Gem.lua`, sets `ATTR_key = "ATK"` and `plus = 1`, and assigns the gem texture.
- Both Blueprints keep `getSE` at its metadata default so runtime pickup resolves `Audio.getSE`.

> **Note:** These template mixins rely on their declared parent capabilities, metadata fields and General Data or configuration keys; adapt or replace them if your project uses a custom inventory or attribute model.
