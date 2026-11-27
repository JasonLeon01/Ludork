# GlobalFunctions.Audio

`GlobalFunctions.Audio` provides Blueprint nodes for sound and music playback, channel audio effects and filter configuration. `SoundFilter` and `MusicFilter` hold the shared filters edited by `EditSoundFilter` and `EditMusicFilter`; playback applies them only when `applyFilter` is `true`.

**Metadata source:** `Scripts/GlobalFunctions/Audio_meta.lua`

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `EditSoundFilter` | `attr: string`, `value: any` | — | Sets a field on the shared `SoundFilter`. `attr` drop-down: sound filter attributes (listed below). |
| `EditMusicFilter` | `attr: string`, `value: any` | — | Sets a field on the shared `MusicFilter`. `attr` drop-down: music filter attributes (listed below). |
| `SetEffect` | `audioType: string`, `effect: string` | — | Applies an effect preset to an audio channel. `audioType` drop-down: `Sound`, `Voice`, `Music`. `effect` drop-down: `nil`, `Echo`, `Distortion`, `Underwater`, `BehindWall`. |
| `PlaySound` | `soundFileName: string`, `applyFilter: bool = false` | — | Plays a sound effect, applying `SoundFilter` when `applyFilter` is `true`. Path (`soundFileName`): `/Game/Assets/Sounds`. |
| `PlayMusic` | `musicFileName: string`, `applyFilter: bool = false` | — | Plays background music on the active Scene, applying `MusicFilter` when `applyFilter` is `true`. Path (`musicFileName`): `/Game/Assets/Musics`. |
| `SetBgmFilter` | `attr: string`, `value: any` | — | Updates a filter property on the active `SceneMap` BGM. `attr` drop-down: music filter attributes (listed below). |
| `SetBgsFilter` | `attr: string`, `value: any` | — | Updates a filter property on the active `SceneMap` BGS. `attr` drop-down: sound filter attributes (listed below). |

Filter `attr` drop-down keys:

- **Sound and BGS filters** (`EditSoundFilter`, `SetBgsFilter`): `loop`, `offset`, `pitch`, `pan`, `volume`, `spatial`, `position`, `direction`, `cone`, `velocity`, `dopplerFactor`, `directionalAttenuationFactor`, `relativeToListener`, `minDistance`, `maxDistance`, `minGain`, `maxGain`, `attenuation`.
- **Music and BGM filters** (`EditMusicFilter`, `SetBgmFilter`): all sound filter keys above plus `loopPoint`.
