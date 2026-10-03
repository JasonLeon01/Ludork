# GlobalCore: UI, Weather and Module Root

## UIManager

Direct metadata bases: —

### Stacking and update order

`UIManager` renders and late-updates in ascending `Canvas.zOrder`, but runs logic and fixed updates in reverse order, so that foreground UIs consume input first. At equal z-order, the later-loaded UI is in front.

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `getFocusManager` | `function` | self: { "GlobalCore", "UIManager" } = "self" | return: { "GlobalCore", "FocusManager" } | Pure |
| `setFocusNavigationEnabled` | `function` | self: { "GlobalCore", "UIManager" } = "self"; enabled: bool | — | — |
| `registerFocusGroup` | `function` | self: { "GlobalCore", "UIManager" } = "self"; group: { "GlobalCore", "FocusGroup" } | — | — |
| `loadUI` | `function` | self: { "GlobalCore", "UIManager" } = "self"; ui: { "Engine", "ControlBase" } | — | ExecSplit { [1] = "default", default = nil } |
| `getUIs` | `function` | self: { "GlobalCore", "UIManager" } = "self" | uis: { "Engine", "ControlBase[]" } | Pure |
| `removeUI` | `function` | self: { "GlobalCore", "UIManager" } = "self"; ui: { "Engine", "ControlBase" } | — | ExecSplit { [1] = "default", default = nil } |

## WeatherType

`GlobalCore.WeatherType` defines `NONE = 0`, `RAIN = 1`, `STORM = 2` and `SNOW = 3`. The native build exports the same values through the pure-data module `Enums.GlobalCore.WeatherType`. Lua signatures retain `GlobalCore.WeatherType`; Blueprint inputs and outputs use `{ enum = "Enums.GlobalCore.WeatherType" }`, showing keys and storing the underlying integers.

## WeatherController

Direct metadata bases: —

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `setWeather` | `function` | weatherType: GlobalCore.WeatherType; power: float; maxCount: int | — | — |
| `clearWeather` | `function` | — | — | — |
| `update` | `function` | deltaTime: float | — | — |
| `drawShaderOverlay` | `function` | camera: { "GlobalCore", "Camera" } | — | — |
| `getWeatherType` | `function` | — | return: GlobalCore.WeatherType | Pure |

## GlobalCore

Direct metadata bases: —

### Properties

No editable properties are declared. `WeatherType` is a Lua enum table, not a Blueprint property.

### Functions and events

No Blueprint functions or events are declared.
