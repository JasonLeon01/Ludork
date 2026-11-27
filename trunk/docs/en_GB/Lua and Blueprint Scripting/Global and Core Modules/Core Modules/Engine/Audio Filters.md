# Engine: Audio Filters

This page documents audio playback filter records (`SoundFilter`, `MusicFilter`) and Actor auto-sound settings (`AutoSoundParams`) under `Engine`. These types are pure-property configuration records with no instance methods.

## SoundFilter

Configures playback pitch, pan, volume, looping and 3D spatial attenuation for `sf.Sound` instances played through `AudioManager` or `GlobalFunctions.Manager`.

### Properties

| Field | Type | Default | Notes |
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

## MusicFilter

**Inherits:** `Engine.SoundFilter`

Extends `SoundFilter` with a loop-point time span for streaming `sf.Music` playback.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `loopPoint` | `sf.Music.TimeSpan` | — | — |

## AutoSoundParams

Configures spatial playback defaults for an `Engine.Actor`'s `autoSoundParams` property.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `volume` | `float` | `100` | — |
| `minDistance` | `float` | `64` | — |
| `attenuation` | `float` | `1` | — |
| `loop` | `bool` | `false` | — |
| `maxDistance` | `float` | `0` | — |
