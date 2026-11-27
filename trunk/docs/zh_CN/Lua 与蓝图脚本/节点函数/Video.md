# GlobalFunctions.Video

`GlobalFunctions.Video` 提供 `PlayVideo` 蓝图节点，用于播放视频并支持可选的静音、跳过与字幕参数。

**元数据来源**：`Scripts/GlobalFunctions/Video_meta.lua`

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `PlayVideo` | `videoFileName: string`, `mute: bool = false`, `skipable: bool = true`, `subtitleFileName: string = ""` | — | 播放视频文件。路径约束（`videoFileName`）：`/Game/Assets/Videos`；路径约束（`subtitleFileName`）：`Data/Subtitles`（根目录 `Data`，过滤器 `*.json`）。 |

`subtitleFileName` 为可选的项目相对路径（`Data/Subtitles/...json`）；省略该参数或传入 `""` 时仅播放视频。字幕格式、语言选择与编辑方式见[视频字幕](<../../编辑器用户指南/视频字幕.md>)。
