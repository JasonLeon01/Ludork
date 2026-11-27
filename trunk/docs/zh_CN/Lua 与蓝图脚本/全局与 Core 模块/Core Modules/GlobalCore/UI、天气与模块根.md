# GlobalCore：UI、天气与模块根

本页记录场景中的 `UIManager`、天气枚举与 `WeatherController` 以及 `GlobalCore` 模块根。实例方法表格省略作为首个蓝图目标引脚暴露的隐式 `self` 接收者；静态方法不接收调用对象。

## UIManager

管理场景中活动 UI 根节点与焦点导航。

### 堆叠与更新顺序

`UIManager` 按 `Canvas.zOrder` 升序执行渲染与后期更新（late update），而逻辑更新与固定更新按相反顺序执行，使前景 UI 优先消费输入。`zOrder` 相同时，后加载的 UI 显示在更前层。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getFocusManager` | — | `GlobalCore.FocusManager` | 纯函数（Pure）。 |
| `setFocusNavigationEnabled` | `enabled: bool` | — | — |
| `registerFocusGroup` | `group: GlobalCore.FocusGroup` | — | — |
| `loadUI` | `ui: Engine.ControlBase` | — | 执行引脚（`default`）。 |
| `getUIs` | — | `uis: Engine.ControlBase[]` | 纯函数（Pure）。 |
| `removeUI` | `ui: Engine.ControlBase` | — | 执行引脚（`default`）。 |

## WeatherType

`GlobalCore.WeatherType` 定义前景天气叠加模式，纯数据模块 `Enums.GlobalCore.WeatherType` 导出完全相同的常量值：

| 常量 | 值 |
|---|---|
| `NONE` | `0` |
| `RAIN` | `1` |
| `STORM` | `2` |
| `SNOW` | `3` |

Lua 签名使用 `GlobalCore.WeatherType`。蓝图输入与输出声明 `{ enum = "Enums.GlobalCore.WeatherType" }`，界面显示枚举键名，底层存储整数值。

## WeatherController

用于控制前景全屏天气叠加的静态控制器。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `setWeather` | `weatherType: GlobalCore.WeatherType`, `power: float`, `maxCount: int` | — | 静态方法。 |
| `clearWeather` | — | — | 静态方法。 |
| `update` | `deltaTime: float` | — | 静态方法。 |
| `drawShaderOverlay` | `camera: GlobalCore.Camera` | — | 静态方法。 |
| `getWeatherType` | — | `GlobalCore.WeatherType` | 静态方法。纯函数（Pure）。 |

## GlobalCore

### 属性

未声明任何可编辑属性。`WeatherType` 是 Lua 枚举表，而非蓝图属性。
