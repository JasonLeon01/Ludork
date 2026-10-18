# GlobalFunctions.Video

`Video` declares the node that plays a video, with optional mute, skip and subtitle arguments.

Metadata source: `Scripts/GlobalFunctions/Video_meta.lua`

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `PlayVideo` | `function` | videoFileName: string; mute: bool = false; skipable: bool = true; subtitleFileName: string = "" | — | ExecSplit { [1] = "default", default = nil }; Meta { PathRoot = { subtitleFileName = "Data" }; PathFilter = { subtitleFileName = "*.json" }; PathVars = { [1] = { [1] = "videoFileName", [2] = "/Game/Assets/Videos" }, [2] = { [1] = "subtitleFileName", [2] = "Data/Subtitles" } } } |

`subtitleFileName` is an optional project-relative `Data/Subtitles/...json` path; omit it or pass `""` for video-only playback. See [Video Subtitles](<../../Editor User Guide/Video Subtitles.md>) for format, language selection and editing.
