# GlobalFunctions.String

`GlobalFunctions.String` provides pure Blueprint nodes for converting values to and from strings and performing search, replacement, splitting, casing and substring operations.

**Metadata source:** `Scripts/GlobalFunctions/String_meta.lua`

## String

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `ToString` | `value: any = ""` | `value: string` | Pure. Converts `value` to a string. |
| `GetIntFromStr` | `value: string = "0"` | `value: int` | Pure. Parses an integer from `value`. |
| `GetFloatFromStr` | `value: string = "0"` | `value: float` | Pure. Parses a float from `value`. |
| `StringConcat` | `str1: string = ""`, `str2: string = ""` | `value: string` | Pure. Concatenates `str1` and `str2`. |
| `StringContains` | `str1: string = ""`, `str2: string = ""` | `value: bool` | Pure. Checks whether `str1` contains `str2`. |
| `StringLength` | `str1: string = ""` | `value: int` | Pure. Returns the length of `str1`. |
| `StringFind` | `str1: string = ""`, `str2: string = ""` | `value: int` | Pure. Finds the position of `str2` in `str1`. |
| `StringReplace` | `str1: string = ""`, `str2: string = ""`, `str3: string = ""` | `value: string` | Pure. Replaces occurrences of `str2` in `str1` with `str3`. |
| `StringSplit` | `str1: string = ""`, `str2: string = ","` | `value: string[]` | Pure. Splits `str1` using separator `str2`. |
| `StringSubstring` | `str1: string = ""`, `start: int = 0`, `finish: int = 0` | `value: string` | Pure. Extracts the substring between `start` and `finish`. |
| `StringToLower` | `str1: string = ""` | `value: string` | Pure. Converts `str1` to lower case. |
| `StringToUpper` | `str1: string = ""` | `value: string` | Pure. Converts `str1` to upper case. |
| `StringStrip` | `str1: string = ""` | `value: string` | Pure. Strips leading and trailing whitespace from `str1`. |
