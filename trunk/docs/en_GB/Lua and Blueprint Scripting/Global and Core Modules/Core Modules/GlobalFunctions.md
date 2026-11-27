# GlobalFunctions Lua API

`GlobalFunctions` exposes native free functions organised into the `Components`, `Manager`, `NodeGraph` and `UI` group tables. All functions on this page are Lua-only static helpers with no `self` receiver and do not create Blueprint nodes (`Scripts/GlobalFunctions_meta.lua` is empty).

Declarations live in `Scripts/stub/GlobalFunctions.d.lua` and `Engine/Source/GlobalFunctions/include/GlobalFunctions`. Each grouped function exists only on its group table. Lua Blueprint function modules under `Scripts/GlobalFunctions` (such as `require("GlobalFunctions.Math")`) load separately and are not members of this native root table.

## Loading

See [Lua Runtime and Modules](<../../Lua Runtime and Modules.md#entry-and-module-order>) for module loading order.

## Components

The `Components` group supports class metadata, serialisation and component attachment. Functions prefixed with `_` and functions marked `metadata = false` are runtime infrastructure; project gameplay should use the public class and component workflow instead.

> **Note:** `getComponentFieldValue` and `setComponentFieldValue` require the declared component instance to be present. A missing or wrong-type component raises an error instead of rebuilding it.

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `_cloneComponentValue` | `value: any`, `valueType: any = nil`, `declaringModule: any = nil` | `any` | Runtime infrastructure. |
| `_cloneComponentFieldValue` | `componentType: any`, `fieldName: string`, `value: any` | `any` | Runtime infrastructure. |
| `isComponentType` | `value: any` | `bool` | — |
| `getComponentTypes` | `classValue: any` | `table` | — |
| `getComponentFieldDefaults` | `componentType: any` | `table` | — |
| `getComponentFieldMap` | `classValue: any` | `table` | — |
| `componentFromData` | `componentType: any`, `data: any` | `any` | — |
| `componentToData` | `value: any` | `table` | — |
| `_getComponentFieldTarget` | `object: any`, `fieldName: string` | `any`, `string?`, `any` | Runtime infrastructure. |
| `getComponentFieldValue` | `object: any`, `fieldName: string`, `defaultValue: any = nil` | `any` | — |
| `setComponentFieldValue` | `object: any`, `fieldName: string`, `value: any` | `bool` | — |
| `_isBlankValue` | `value: any` | `bool` | Runtime infrastructure. |
| `_mergeComponentDefaults` | `object: any` | — | Runtime infrastructure. |
| `normaliseInstanceComponents` | `object: any` | — | — |
| `attachInstanceComponents` | `object: any` | `any[]` | — |

## Manager

Convenience wrappers around the native audio, font, texture and shader resource managers.

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `getSoundBuffer` | `filePath: string` | `sf.SoundBuffer` | Requires a complete `/Game/Assets/...` logical path. |
| `playSE` | `filename: string`, `filter: Engine.SoundFilter = nil` | `sf.Sound` | Category-relative path. |
| `playVoice` | `filename: string`, `filter: Engine.SoundFilter = nil`, `refActor: sf.Transformable = nil`, `minDistance: float = 64` | `sf.Sound` | Category-relative path. |
| `playMusic` | `musicType: string`, `filename: string`, `filter: Engine.MusicFilter = nil` | `sf.Music` | Category-relative path. |
| `stopSound` | — | — | — |
| `stopVoice` | — | — | — |
| `stopMusic` | `musicType: string` | — | — |
| `loadFont` | `filename: string` | `sf.Font` | Category-relative path. |
| `getFont` | `fontName: string` | `sf.Font` | — |
| `getFontFilename` | `fontName: string` | `string` | — |
| `getFontList` | — | `string[]` | — |
| `getFontFilenameList` | — | `string[]` | — |
| `hasFont` | `fontName: string` | `bool` | — |
| `loadTexture` | `subFolder: string`, `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | Category-relative path. |
| `loadBlock` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | Category-relative path. |
| `loadCharacter` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | Category-relative path. |
| `loadSystem` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | Category-relative path. |
| `loadTileset` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | Category-relative path. |
| `loadAutotile` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | Category-relative path. |
| `loadFog` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | Category-relative path. |
| `loadTransition` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | Category-relative path. |
| `loadShader` | `shaderPath: string`, `shaderType: sf.Shader.Type = nil` | `sf.Shader` | Category-relative path. |
| `loadFullShaderWithGeo` | `vertPath: string`, `geoPath: string`, `fragPath: string` | `sf.Shader` | Category-relative paths. |
| `loadGeoShader` | `vertPath: string`, `geoPath: string`, `fragPath: string` | `sf.Shader` | Category-relative paths. |

## NodeGraph

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `initLatent` | — | — | Initialises latent-node support; runtime infrastructure (`metadata = false`). |

## UI

`HexColor` is overloaded for a hexadecimal string or a packed integer. Both overloads default to `alpha = 255` when the input does not specify an alpha channel.

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `HexColor` | `value: string`, `alpha: int = 255` | `sf.Color` | String overload. |
| `HexColor` | `value: int`, `alpha: int = 255` | `sf.Color` | Packed integer overload. |
| `GetRosyBrown` | — | `sf.Color` | — |
| `GetCopper` | — | `sf.Color` | — |
| `GetSage` | — | `sf.Color` | — |
| `GetTeal` | — | `sf.Color` | — |
| `GetMutedPurple` | — | `sf.Color` | — |
| `GetTaupe` | — | `sf.Color` | — |
| `GetTerraCotta` | — | `sf.Color` | — |
| `GetOchre` | — | `sf.Color` | — |
| `GetFernGreen` | — | `sf.Color` | — |
| `GetSteelBlue` | — | `sf.Color` | — |
| `GetDimGrey` | — | `sf.Color` | — |
| `GetCharcoal` | — | `sf.Color` | — |

## Limitations

Category helpers accept only category-relative paths and prepend their `/Game/Assets/...` prefix (for example `loadCharacter("actors/Hero.png")` and `playSE("cursor.ogg")`). Stored data and `getSoundBuffer(filePath)` require complete logical paths instead. Loading failures follow the native manager contract.
