# Game Settings Reference

**Game Config** edits the supported keys in the `Main` section of the [platform game configuration](<../../Editor User Guide/Project Settings and Asset Layout.md#game-configuration>). This page documents every key, default, quality preset and platform display constraint used by Default Gameplay.

## Supported keys

| Key | Type | Default and meaning |
|---|---|---|
| `language` | string | Game catalogue key (see [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md>)). |
| `scale` | float | Display-scale preference; default `0` selects Fullscreen on configurable displays. |
| `maxrenderscale` | float | Maximum internal render scale; default `2.0`. Unlimited (`0`) removes the cap. |
| `lightingrenderscale` | discrete float | Direct-light render scale: `0.5`, `0.75` or `1.0` (default `1.0`; invalid values fall back to `1.0`). |
| `framerate` | integer | Target frame rate; default `60`. Unlimited (`0`) removes the cap. |
| `antialiasinglevel` | non-negative integer | Requested SFML multisample anti-aliasing level; default `8`. |
| `verticalsync` | boolean | Enables or disables vertical synchronisation. |
| `musicon`, `soundon`, `voiceon` | boolean | Per-channel audio enable toggles. |
| `musicvolume`, `soundvolume`, `voicevolume` | float | Per-channel volume clamped to `0`–`100`. |

The editor preserves unknown sections and keys. Invalid values for supported keys fall back to safe defaults in their respective readers, though you should fix the source file rather than relying on fallbacks. Saving writes changed settings; new user configurations and missing keys default to the **High** quality preset (`2`, `60`, `8`, `1`) and `scale = 0` without overwriting existing values.

> **Note:** Only keys handled by your runtime readers take effect; adding custom keys to the configuration file requires matching runtime code.

Project and tooling flags live in `Main.proj` rather than mutable user preferences: `Cpp` selects C++ Source behaviour, `ffmpeg` enables video packaging, `IndividualWindow` stores the preferred editor launch mode, and `packaging.version` / `packaging.dev` configure releases (see [Release version and internal packages](<../../Editor User Guide/Run Debug and Package.md#release-version-and-internal-packages>)).

## Display scale, render limits and frame rate

- **Scale (`scale`)**: Applies immediately on configurable displays. Desktop windows and HarmonyOS tablets in free-window mode use positive values for fixed window sizes and `0` for fullscreen; other mobile displays and WebGL omit **Scale** (including Fullscreen), while **Render Limit** is available. A newly created config writes `scale = 0`, whereas a missing key uses `0` in memory only. Negative or non-finite values resolve to `1`. See [Display](<../Global and Core Modules/Core Modules/GlobalCore/Scenes and System.md#display>) for runtime APIs.
- **Desktop and HarmonyOS 2-in-1 scale filtering**: Startup filters the positive presets (`1`, `1.25`, `1.5`, `1.75`, `2`, `3`, `4`) against the primary display work area, and opening Config rechecks the current display. Fullscreen (`0`) and fitting non-preset values stay available; an over-limit value clamps down to the highest fitting preset (or Fullscreen if none fits) and does not raise automatically on a larger display. A failed size query leaves choices and the current value unchanged.
- **HarmonyOS tablet free-window limits**: Free-window mode uses the smaller of the display's available area and the Stage maximum. That maximum is sampled at native host initialisation, when enabling free-window mode and on returning to the foreground — moving windows across displays while staying in the foreground does not resample it. HarmonyOS 2-in-1 follows ordinary desktop display queries.
- **Render Limit (`maxrenderscale`)**: Caps the internal Graphics canvas and dependent render targets immediately on all platforms without changing window dimensions, logical game size or `scale`. Measured in physical drawable pixels relative to logical game size (including Retina density); on macOS, `scale` controls window size in Cocoa points while Unlimited renders at full drawable resolution. Config offers `0.5`, `0.75`, `1`, `1.5`, `2`, `3`, `4` and Unlimited (`0`), preserves any other finite non-negative value, and resolves negative or non-finite values to `2`.
- **Lighting Render Scale (`lightingrenderscale`)**: Adjusts direct-light resolution starting on the next map frame without resizing the window or canvas; values below `1.0` smoothly upsample. Missing, non-finite or unsupported values resolve to `1.0`. See [Graphics](<../Global and Core Modules/Core Modules/GlobalCore/Scenes and System.md#graphics>).
- **Frame Rate (`framerate`)**: Offers `30`, `60`, `90`, `120` and Unlimited (`0`). Unlimited disables active frame-rate capping while preserving **Vertical Sync**. Fixed updates step at `1/60` second when Unlimited, or `1/framerate` for positive frame rates.
- **Anti-aliasing (`antialiasinglevel`)**: Requests a multisample level (SFML selects the closest supported context; `0` disables anti-aliasing, and negative or out-of-range values fall back to the SFML default). Because anti-aliasing applies at context creation, changing it saves immediately and takes effect after display reinitialisation (normally on the next launch).

## Quality presets

Config derives the displayed **Quality Preset** from `(maxrenderscale, framerate, antialiasinglevel, lightingrenderscale)` in the order below:

| Preset | `(maxrenderscale, framerate, antialiasinglevel, lightingrenderscale)` |
|---|---|
| Low | `(0.75, 30, 0, 0.5)` |
| Medium | `(1, 60, 2, 0.75)` |
| High | `(2, 60, 8, 1)` |
| Extra High | `(3, 90, 8, 1)` |
| Original | `(0 (Unlimited), 120, 8, 1)` |

If the active values match no preset, Config appends and selects **Custom** (selecting Custom applies no changes); once values match a preset again, Config selects it and removes **Custom**. Presets are derived dynamically — no preset key is saved, and `scale` and `verticalsync` do not participate.
