# GlobalFunctions.Save

`GlobalFunctions.Save` 提供用于保存游戏进度、从存档文件读取进度以及解析编号存档槽文件路径的蓝图节点。

**元数据来源**：`Scripts/GlobalFunctions/Save_meta.lua`

## Save

### 函数与事件

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `SaveGame` | `filePath: string = ""` | — | 将当前游戏状态保存至 `filePath`。 |
| `LoadGame` | `filePath: string = ""` | `int` | 执行分支：`Loaded`（`0`）、`NotFound`（`1`）。从 `filePath` 读取游戏存档。 |
| `GetSavePath` | `slot: int = 1` | `path: string` | 纯函数（Pure）。解析编号 `slot` 对应的存档文件路径。 |
