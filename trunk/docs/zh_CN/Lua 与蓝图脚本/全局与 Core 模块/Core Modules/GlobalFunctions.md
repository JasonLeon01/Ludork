# GlobalFunctions Lua API

`GlobalFunctions` 通过 `Components`、`Manager`、`NodeGraph` 与 `UI` 四个分组表暴露原生自由函数。本页所有函数均为无 `self` 接收者的纯 Lua 静态辅助函数，不生成蓝图节点（`Scripts/GlobalFunctions_meta.lua` 为空）。

声明位于 `Scripts/stub/GlobalFunctions.d.lua` 与 `Engine/Source/GlobalFunctions/include/GlobalFunctions`，每个分组函数仅存在于各自对应的分组表中。`Scripts/GlobalFunctions` 下的 Lua 蓝图函数模块（例如 `require("GlobalFunctions.Math")`）作为独立模块加载，不属于此原生根表。

## 加载

模块加载顺序见 [Lua 运行时与模块](<../../Lua 运行时与模块.md#入口与模块顺序>)。

## Components

`Components` 分组支持类元数据、序列化与组件挂载。以 `_` 开头或标记为 `metadata = false` 的函数属于运行时基础设施，项目玩法逻辑应使用公开的类与组件工作流。

> **注意**：`getComponentFieldValue` 与 `setComponentFieldValue` 要求已声明的组件实例存在。组件缺失或类型不匹配时会直接抛出错误，而不会自动重建。

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `_cloneComponentValue` | `value: any`, `valueType: any = nil`, `declaringModule: any = nil` | `any` | 运行时基础设施。 |
| `_cloneComponentFieldValue` | `componentType: any`, `fieldName: string`, `value: any` | `any` | 运行时基础设施。 |
| `isComponentType` | `value: any` | `bool` | — |
| `getComponentTypes` | `classValue: any` | `table` | — |
| `getComponentFieldDefaults` | `componentType: any` | `table` | — |
| `getComponentFieldMap` | `classValue: any` | `table` | — |
| `componentFromData` | `componentType: any`, `data: any` | `any` | — |
| `componentToData` | `value: any` | `table` | — |
| `_getComponentFieldTarget` | `object: any`, `fieldName: string` | `any`, `string?`, `any` | 运行时基础设施。 |
| `getComponentFieldValue` | `object: any`, `fieldName: string`, `defaultValue: any = nil` | `any` | — |
| `setComponentFieldValue` | `object: any`, `fieldName: string`, `value: any` | `bool` | — |
| `_isBlankValue` | `value: any` | `bool` | 运行时基础设施。 |
| `_mergeComponentDefaults` | `object: any` | — | 运行时基础设施。 |
| `normaliseInstanceComponents` | `object: any` | — | — |
| `attachInstanceComponents` | `object: any` | `any[]` | — |

## Manager

原生音频、字体、纹理与着色器资源管理器的便捷封装。

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getSoundBuffer` | `filePath: string` | `sf.SoundBuffer` | 要求完整的 `/Game/Assets/...` 逻辑路径。 |
| `playSE` | `filename: string`, `filter: Engine.SoundFilter = nil` | `sf.Sound` | 类别相对路径。 |
| `playVoice` | `filename: string`, `filter: Engine.SoundFilter = nil`, `refActor: sf.Transformable = nil`, `minDistance: float = 64` | `sf.Sound` | 类别相对路径。 |
| `playMusic` | `musicType: string`, `filename: string`, `filter: Engine.MusicFilter = nil` | `sf.Music` | 类别相对路径。 |
| `stopSound` | — | — | — |
| `stopVoice` | — | — | — |
| `stopMusic` | `musicType: string` | — | — |
| `loadFont` | `filename: string` | `sf.Font` | 类别相对路径。 |
| `getFont` | `fontName: string` | `sf.Font` | — |
| `getFontFilename` | `fontName: string` | `string` | — |
| `getFontList` | — | `string[]` | — |
| `getFontFilenameList` | — | `string[]` | — |
| `hasFont` | `fontName: string` | `bool` | — |
| `loadTexture` | `subFolder: string`, `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | 类别相对路径。 |
| `loadBlock` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | 类别相对路径。 |
| `loadCharacter` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | 类别相对路径。 |
| `loadSystem` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | 类别相对路径。 |
| `loadTileset` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | 类别相对路径。 |
| `loadAutotile` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | 类别相对路径。 |
| `loadFog` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | 类别相对路径。 |
| `loadTransition` | `filename: string`, `sRGB: bool = false`, `area: sf.IntRect = nil`, `smooth: bool = false` | `sf.Texture` | 类别相对路径。 |
| `loadShader` | `shaderPath: string`, `shaderType: sf.Shader.Type = nil` | `sf.Shader` | 类别相对路径。 |
| `loadFullShaderWithGeo` | `vertPath: string`, `geoPath: string`, `fragPath: string` | `sf.Shader` | 类别相对路径。 |
| `loadGeoShader` | `vertPath: string`, `geoPath: string`, `fragPath: string` | `sf.Shader` | 类别相对路径。 |

## NodeGraph

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `initLatent` | — | — | 初始化潜伏节点支持；属于运行时基础设施（`metadata = false`）。 |

## UI

`HexColor` 提供十六进制字符串与打包整数两个重载。输入未包含 Alpha 通道时，两个重载均默认 `alpha = 255`。

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `HexColor` | `value: string`, `alpha: int = 255` | `sf.Color` | 字符串重载。 |
| `HexColor` | `value: int`, `alpha: int = 255` | `sf.Color` | 打包整数重载。 |
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

## 限制

各类别辅助函数仅接受对应类别内的相对路径，并自动补全 `/Game/Assets/...` 前缀（例如 `loadCharacter("actors/Hero.png")` 与 `playSE("cursor.ogg")`）。持久化数据与 `getSoundBuffer(filePath)` 则要求完整的逻辑路径。加载失败时的行为遵循对应原生资源管理器的契约。
