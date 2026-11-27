# Equipment and Items

Create Item and Equipment General Data entries alongside `Source.MapActors.Item` and `Source.MapActors.Equip` Blueprints with stable inventory IDs, matching Class slots and focused member graphs.

## Author items and equipment

- **Item activation and stacking**: Set `usable` on Item General Data to enable inventory use, and `cost` to display stack counts. The `onUse` graph decides whether to consume the item.
- **Item member graphs**: Put inventory activation logic in `onUse` and enemy drop logic in `onDrop` (see [Items](<../Actors Enemies Items and Equipment.md#items>) for `GameplayEventData.target` and `self` pin rules).
- **Equipment slots and stats**: Match each Equip record's `slot` to the active Class slot dictionary. Put static stat bonuses in `attrPlus` and reserve `onEquip` / `onUnequip` graphs for custom side effects.
- **Icons**: Store item and equipment icons under `Assets/Icons` and select them through the resource picker.

## Default template examples

- **Equipment (`BP_Equip_Sword_A.json`)**: Derives from `Source.MapActors.Equip` and sets `ID = "Sword_A"` to link its Equip record. See [Equipment](<../Actors Enemies Items and Equipment.md#equipment>) for the execution order of `attrPlus`, `onEquip` and `onUnequip`.
- **Item (`BP_Item_Key_Y.json`)**: Derives from `Source.MapActors.Item` and sets `ID = "KEY_Y"`, matching both the Item database record and the yellow door's `needKeyID`.
- **Ability ownership and save loading**: Owning an item grants its `onUse` ability and removing the last copy revokes it; `Player:activateItem` targets that Player. [Save loading](<../Runtime Data Configuration and Saves.md#players>) restores saved abilities and Effects without replaying `onUse`, `onDrop`, `onEquip` or `onUnequip` graphs.

> **Important:** Renaming a General Data member does not update stored Blueprint or map references automatically. Class slot mismatches and unknown record IDs raise explicit data errors.
