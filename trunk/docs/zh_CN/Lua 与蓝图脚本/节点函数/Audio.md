# GlobalFunctions.Audio

`GlobalFunctions.Audio` 提供用于音效与音乐播放、声道音频效果及过滤器配置的蓝图节点。`SoundFilter` 与 `MusicFilter` 保存由 `EditSoundFilter` 和 `EditMusicFilter` 修改的共享过滤器，播放节点仅在 `applyFilter` 为 `true` 时应用这些过滤器。

**元数据来源**：`Scripts/GlobalFunctions/Audio_meta.lua`

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `EditSoundFilter` | `attr: string`, `value: any` | — | 设置共享 `SoundFilter` 的指定字段。`attr` 下拉选项见下方音效过滤器属性列表。 |
| `EditMusicFilter` | `attr: string`, `value: any` | — | 设置共享 `MusicFilter` 的指定字段。`attr` 下拉选项见下方音乐过滤器属性列表。 |
| `SetEffect` | `audioType: string`, `effect: string` | — | 为指定音频声道应用效果预设。`audioType` 下拉选项：`Sound`、`Voice`、`Music`；`effect` 下拉选项：`nil`、`Echo`、`Distortion`、`Underwater`、`BehindWall`。 |
| `PlaySound` | `soundFileName: string`, `applyFilter: bool = false` | — | 播放音效；`applyFilter` 为 `true` 时应用 `SoundFilter`。路径约束（`soundFileName`）：`/Game/Assets/Sounds`。 |
| `PlayMusic` | `musicFileName: string`, `applyFilter: bool = false` | — | 在当前活动场景播放背景音乐；`applyFilter` 为 `true` 时应用 `MusicFilter`。路径约束（`musicFileName`）：`/Game/Assets/Musics`。 |
| `SetBgmFilter` | `attr: string`, `value: any` | — | 更新当前活动 `SceneMap` 的 BGM 过滤器属性。`attr` 下拉选项见下方音乐过滤器属性列表。 |
| `SetBgsFilter` | `attr: string`, `value: any` | — | 更新当前活动 `SceneMap` 的 BGS 过滤器属性。`attr` 下拉选项见下方音效过滤器属性列表。 |

过滤器 `attr` 下拉键列表：

- **音效与 BGS 过滤器**（`EditSoundFilter`、`SetBgsFilter`）：`loop`、`offset`、`pitch`、`pan`、`volume`、`spatial`、`position`、`direction`、`cone`、`velocity`、`dopplerFactor`、`directionalAttenuationFactor`、`relativeToListener`、`minDistance`、`maxDistance`、`minGain`、`maxGain`、`attenuation`。
- **音乐与 BGM 过滤器**（`EditMusicFilter`、`SetBgmFilter`）：包含上述全部音效过滤器键，并额外支持 `loopPoint`。
