# GlobalFunctions.Video

`GlobalFunctions.Video` provides the `PlayVideo` Blueprint node for video playback with optional mute, skip and subtitle arguments.

**Metadata source:** `Scripts/GlobalFunctions/Video_meta.lua`

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `PlayVideo` | `videoFileName: string`, `mute: bool = false`, `skipable: bool = true`, `subtitleFileName: string = ""` | — | Plays a video file. Path (`videoFileName`): `/Game/Assets/Videos`. Path (`subtitleFileName`): `Data/Subtitles` (root `Data`, filter `*.json`). |

`subtitleFileName` is an optional project-relative `Data/Subtitles/...json` path; omit it or pass `""` for video-only playback. See [Video Subtitles](<../../Editor User Guide/Video Subtitles.md>) for format, language selection and editing.
