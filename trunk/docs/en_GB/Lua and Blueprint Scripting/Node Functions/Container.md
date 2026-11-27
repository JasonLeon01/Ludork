# GlobalFunctions.Container

`GlobalFunctions.Container` provides Blueprint loop constructs alongside dictionary and list creation, lookup, mutation and table-conversion nodes.

**Metadata source:** `Scripts/GlobalFunctions/Container_meta.lua`

## Container

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `ForLoop` | `firstIndex: int = 0`, `lastIndex: int = 0`, `step: int = 1` | `index: int` | Loop (`ForLoop`). Branches: `LoopBody` (`__loop_body__`), `Completed` (`__loop_completed__`). |
| `ForEach` | `list_: Union[any[], _G.list]` | `element: any`, `index: int` | Loop (`ForEach`). Branches: `LoopBody` (`__loop_body__`), `Completed` (`__loop_completed__`). |
| `CreateDict` | — | `value: any` | Pure. Creates an empty dictionary. |
| `DictGet` | `dict_: any`, `key: any` | `value: any` | Pure. Reads the value stored at `key`. |
| `DictAdd` | `dict_: any`, `key: any`, `value: any` | — | Inserts or updates `key` with `value`. |
| `DictRemove` | `dict_: any`, `key: any` | — | Removes `key` from the dictionary. |
| `DictClear` | `dict_: any` | — | Removes all entries from the dictionary. |
| `DictContains` | `dict_: any`, `key: any` | `value: bool` | Pure. Returns whether `key` exists in the dictionary. |
| `TableToDict` | `table_: any` | `value: _G.dict` | Pure. Converts a Lua table to `_G.dict`. |
| `DictToTable` | `dict_: _G.dict` | `value: any` | Pure. Converts a `_G.dict` to a Lua table. |
| `CreateList` | — | `value: any[]` | Pure. Creates an empty list. |
| `ListGet` | `list_: Union[any[], _G.list, _G.tuple]`, `index: int = 0` | `value: any` | Pure. Reads the element at `index`. |
| `ListAppend` | `list_: Union[any[], _G.list]`, `value: any` | — | Appends `value` to the end of the list. |
| `ListExtend` | `list_: Union[any[], _G.list]`, `values: Union[any[], _G.list, _G.tuple]` | — | Appends all elements from `values` to the list. |
| `ListRemove` | `list_: Union[any[], _G.list]`, `index: int = 0` | — | Removes the element at `index`. |
| `ListFind` | `list_: Union[any[], _G.list]`, `value: any` | `index: int` | Pure. Finds the index of `value` in the list. |
| `ListClear` | `list_: Union[any[], _G.list]` | — | Removes all elements from the list. |
| `ListContains` | `list_: Union[any[], _G.list]`, `value: any` | `value: bool` | Pure. Returns whether `value` is present in the list. |
| `TableToList` | `table_: any[]` | `value: _G.list` | Pure. Converts a Lua table to `_G.list`. |
| `ListToTable` | `list_: _G.list` | `value: any[]` | Pure. Converts a `_G.list` to a Lua table. |
