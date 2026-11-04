# Equipment and Items

Create item and equipment records with stable inventory IDs, valid slots and focused member-graph behaviour.

## Prerequisites

- Item, Equip and Class General Data with the required inventory and slot identifiers.
- Blueprint parents derived from `Source.MapActors.Item` or `Source.MapActors.Equip`.

## Steps

- Use Item `usable` to allow inventory activation and `cost` to display its stack count. The `onUse` graph decides whether to consume an Item.
- Put inventory use in `onUse` and enemy drop behaviour in `onDrop`. See [Items](<../Actors Enemies Items and Equipment.md#items>) for graph targets and receivers.
- Match the Equip `slot` to the selected Class slot dictionary.
- Store stat changes in `attrPlus`. Reserve the `onEquip` and `onUnequip` graphs for special behaviour.
- Keep icons under `Assets/Icons` and use the resource selector.

## Game equipment

`BP_Equip_Sword_A.json` derives from `Source.MapActors.Equip` and sets `ID = "Sword_A"`, which resolves the Equip member.

The [equipment runtime rules](<../Actors Enemies Items and Equipment.md#equipment>) define when `attrPlus`, `onEquip` and `onUnequip` take effect.

## Game item

`BP_Item_Key_Y.json` derives from `Source.MapActors.Item` and sets `ID = "KEY_Y"`, matching both the Item member and the yellow door's `needKeyID`.

Owning an Item grants its `onUse` ability; removing the last copy revokes it. `Player:activateItem` targets that Player. [Loading](<../Runtime Data Configuration and Saves.md#players>) rebuilds saved abilities and Effects without replaying member graphs.

## Limitations

Renaming a member does not repair stored references automatically. A Class slot mismatch and an unknown record ID remain explicit data errors.

## Related pages

- [Item](<../../../Editor User Guide/General Data Reference/Item.md>)
- [Equip](<../../../Editor User Guide/General Data Reference/Equip.md>)
