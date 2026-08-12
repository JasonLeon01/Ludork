# Engine: Audio Filters

## SoundFilter

Direct metadata bases: —

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `loop` | `bool` | — | — |
| `offset` | `sf.Time` | — | — |
| `pitch` | `float` | — | — |
| `pan` | `float` | — | — |
| `volume` | `float` | — | — |
| `spatial` | `bool` | — | — |
| `position` | `sf.Vector3f` | — | — |
| `direction` | `sf.Vector3f` | — | — |
| `cone` | `sf.SoundSource.Cone` | — | — |
| `velocity` | `sf.Vector3f` | — | — |
| `dopplerFactor` | `float` | — | — |
| `directionalAttenuationFactor` | `float` | — | — |
| `relativeToListener` | `bool` | — | — |
| `minDistance` | `float` | — | — |
| `maxDistance` | `float` | — | — |
| `minGain` | `float` | — | — |
| `maxGain` | `float` | — | — |
| `attenuation` | `float` | — | — |

### Functions and events

No Blueprint functions or events are declared.

## MusicFilter

Direct metadata bases: `{ "Engine", "SoundFilter" }`

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `loopPoint` | `sf.Music.TimeSpan` | — | — |

### Functions and events

No Blueprint functions or events are declared.

## AutoSoundParams

Direct metadata bases: —

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `volume` | `float` | `100` | — |
| `minDistance` | `float` | `64` | — |
| `attenuation` | `float` | `1` | — |
| `loop` | `bool` | `false` | — |
| `maxDistance` | `float` | `0` | — |

### Functions and events

No Blueprint functions or events are declared.
