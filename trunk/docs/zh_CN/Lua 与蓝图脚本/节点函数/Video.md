# GlobalFunctions.Video

`Video` 声明播放视频的节点，并提供可选的静音、可跳过与字幕参数。

Metadata 来源：`Scripts/GlobalFunctions/Video_meta.lua`

| 名称 | 种类 | 参数 | 返回 | 执行语义与 metadata |
|---|---|---|---|---|
| `PlayVideo` | `function` | videoFileName: string; mute: bool = false; skipable: bool = true; subtitleFileName: string = "" | — | ExecSplit { [1] = "default", default = nil }; Meta { PathRoot = { subtitleFileName = "Data" }; PathFilter = { subtitleFileName = "*.json" }; PathVars = { [1] = { [1] = "videoFileName", [2] = "/Game/Assets/Videos" }, [2] = { [1] = "subtitleFileName", [2] = "Data/Subtitles" } } } |

`subtitleFileName` 是可选的项目相对路径 `Data/Subtitles/...json`；省略或传入 `""` 时只播放视频。格式、语言选择与编辑方法见[视频字幕](<../../编辑器用户指南/视频字幕.md>)。
