# GlobalFunctions.String

`GlobalFunctions.String` 提供面向字符串的类型转换、查找、替换、拆分、大小写转换及子串截取等纯函数蓝图节点。

**元数据来源**：`Scripts/GlobalFunctions/String_meta.lua`

## String

### 函数与事件

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `ToString` | `value: any = ""` | `value: string` | 纯函数（Pure）。将 `value` 转换为字符串。 |
| `GetIntFromStr` | `value: string = "0"` | `value: int` | 纯函数（Pure）。从 `value` 中解析整数。 |
| `GetFloatFromStr` | `value: string = "0"` | `value: float` | 纯函数（Pure）。从 `value` 中解析浮点数。 |
| `StringConcat` | `str1: string = ""`, `str2: string = ""` | `value: string` | 纯函数（Pure）。拼接 `str1` 与 `str2`。 |
| `StringContains` | `str1: string = ""`, `str2: string = ""` | `value: bool` | 纯函数（Pure）。检查 `str1` 是否包含 `str2`。 |
| `StringLength` | `str1: string = ""` | `value: int` | 纯函数（Pure）。返回 `str1` 的长度。 |
| `StringFind` | `str1: string = ""`, `str2: string = ""` | `value: int` | 纯函数（Pure）。查找 `str2` 在 `str1` 中的位置。 |
| `StringReplace` | `str1: string = ""`, `str2: string = ""`, `str3: string = ""` | `value: string` | 纯函数（Pure）。将 `str1` 中出现的 `str2` 替换为 `str3`。 |
| `StringSplit` | `str1: string = ""`, `str2: string = ","` | `value: string[]` | 纯函数（Pure）。使用分隔符 `str2` 拆分 `str1`。 |
| `StringSubstring` | `str1: string = ""`, `start: int = 0`, `finish: int = 0` | `value: string` | 纯函数（Pure）。截取从 `start` 到 `finish` 之间的子串。 |
| `StringToLower` | `str1: string = ""` | `value: string` | 纯函数（Pure）。将 `str1` 转换为小写。 |
| `StringToUpper` | `str1: string = ""` | `value: string` | 纯函数（Pure）。将 `str1` 转换为大写。 |
| `StringStrip` | `str1: string = ""` | `value: string` | 纯函数（Pure）。去除 `str1` 首尾的空白字符。 |
