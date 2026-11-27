# GlobalCore: UI, Weather and Module Root

This page documents the scene `UIManager`, weather types and `WeatherController`, and the `GlobalCore` module root. Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; static methods take no receiver.

## UIManager

Manages active UI roots and focus navigation for a scene.

### Stacking and update order

`UIManager` renders and late-updates in ascending `Canvas.zOrder`, and runs logic and fixed updates in reverse order so foreground UIs consume input first. At equal z-order, the more recently loaded UI sits in front.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getFocusManager` | — | `GlobalCore.FocusManager` | Pure. |
| `setFocusNavigationEnabled` | `enabled: bool` | — | — |
| `registerFocusGroup` | `group: GlobalCore.FocusGroup` | — | — |
| `loadUI` | `ui: Engine.ControlBase` | — | Exec (`default`). |
| `getUIs` | — | `uis: Engine.ControlBase[]` | Pure. |
| `removeUI` | `ui: Engine.ControlBase` | — | Exec (`default`). |

## WeatherType

`GlobalCore.WeatherType` defines the weather overlay modes exported identically by the pure-data module `Enums.GlobalCore.WeatherType`:

| Constant | Value |
|---|---|
| `NONE` | `0` |
| `RAIN` | `1` |
| `STORM` | `2` |
| `SNOW` | `3` |

Lua signatures use `GlobalCore.WeatherType`. Blueprint inputs and outputs declare `{ enum = "Enums.GlobalCore.WeatherType" }`, displaying the key names while storing the underlying integers.

## WeatherController

Static controller for foreground screen weather overlays.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setWeather` | `weatherType: GlobalCore.WeatherType`, `power: float`, `maxCount: int` | — | Static. |
| `clearWeather` | — | — | Static. |
| `update` | `deltaTime: float` | — | Static. |
| `drawShaderOverlay` | `camera: GlobalCore.Camera` | — | Static. |
| `getWeatherType` | — | `GlobalCore.WeatherType` | Static. Pure. |

## GlobalCore

### Properties

No editable properties are declared. `WeatherType` is a Lua enum table rather than a Blueprint property.
