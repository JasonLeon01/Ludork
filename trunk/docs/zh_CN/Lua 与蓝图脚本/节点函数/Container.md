# GlobalFunctions.Container

`GlobalFunctions.Container` 提供蓝图循环结构，以及字典与列表的创建、查询、修改和 Lua 表转换节点。

**元数据来源**：`Scripts/GlobalFunctions/Container_meta.lua`

## Container

### 函数与事件

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `ForLoop` | `firstIndex: int = 0`, `lastIndex: int = 0`, `step: int = 1` | `index: int` | 循环节点（Loop，`ForLoop`）。分支：`LoopBody`（`__loop_body__`）、`Completed`（`__loop_completed__`）。 |
| `ForEach` | `list_: Union[any[], _G.list]` | `element: any`, `index: int` | 循环节点（Loop，`ForEach`）。分支：`LoopBody`（`__loop_body__`）、`Completed`（`__loop_completed__`）。 |
| `CreateDict` | — | `value: any` | 纯函数（Pure）。创建一个空字典。 |
| `DictGet` | `dict_: any`, `key: any` | `value: any` | 纯函数（Pure）。读取字典中 `key` 对应的值。 |
| `DictAdd` | `dict_: any`, `key: any`, `value: any` | — | 插入或更新 `key` 对应的 `value`。 |
| `DictRemove` | `dict_: any`, `key: any` | — | 从字典中移除 `key`。 |
| `DictClear` | `dict_: any` | — | 清空字典中的所有条目。 |
| `DictContains` | `dict_: any`, `key: any` | `value: bool` | 纯函数（Pure）。返回字典中是否存在 `key`。 |
| `TableToDict` | `table_: any` | `value: _G.dict` | 纯函数（Pure）。将 Lua 表转换为 `_G.dict`。 |
| `DictToTable` | `dict_: _G.dict` | `value: any` | 纯函数（Pure）。将 `_G.dict` 转换为 Lua 表。 |
| `CreateList` | — | `value: any[]` | 纯函数（Pure）。创建一个空列表。 |
| `ListGet` | `list_: Union[any[], _G.list, _G.tuple]`, `index: int = 0` | `value: any` | 纯函数（Pure）。读取索引 `index` 处的元素。 |
| `ListAppend` | `list_: Union[any[], _G.list]`, `value: any` | — | 将 `value` 追加至列表末尾。 |
| `ListExtend` | `list_: Union[any[], _G.list]`, `values: Union[any[], _G.list, _G.tuple]` | — | 将 `values` 中的所有元素依次追加至列表末尾。 |
| `ListRemove` | `list_: Union[any[], _G.list]`, `index: int = 0` | — | 移除索引 `index` 处的元素。 |
| `ListFind` | `list_: Union[any[], _G.list]`, `value: any` | `index: int` | 纯函数（Pure）。查找 `value` 在列表中的索引。 |
| `ListClear` | `list_: Union[any[], _G.list]` | — | 清空列表中的所有元素。 |
| `ListContains` | `list_: Union[any[], _G.list]`, `value: any` | `value: bool` | 纯函数（Pure）。返回列表中是否包含 `value`。 |
| `TableToList` | `table_: any[]` | `value: _G.list` | 纯函数（Pure）。将 Lua 表转换为 `_G.list`。 |
| `ListToTable` | `list_: _G.list` | `value: any[]` | 纯函数（Pure）。将 `_G.list` 转换为 Lua 表。 |
