# GlobalFunctions.ScreenEffects

`GlobalFunctions.ScreenEffects` 提供用于触发和清除屏幕闪烁、屏幕色调变换与屏幕震动的蓝图节点。

**元数据来源**：`Scripts/GlobalFunctions/ScreenEffects_meta.lua`

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `FlashScreen` | `red: int = 255`, `green: int = 255`, `blue: int = 255`, `alpha: int = 255`, `duration: float = 0.5` | — | 以指定的 RGBA 颜色触发持续 `duration` 秒的屏幕闪烁。 |
| `StopFlashScreen` | — | — | 停止当前生效的屏幕闪烁。 |
| `ChangeScreenTone` | `red: int = 0`, `green: int = 0`, `blue: int = 0`, `gray: int = 0`, `duration: float = 0` | — | 在 `duration` 秒内将屏幕色调与灰度（`gray`）平滑过渡至目标值。 |
| `ClearScreenTone` | `duration: float = 0` | — | 在 `duration` 秒内清除屏幕色调。 |
| `ScreenShake` | `power: float = 4`, `speed: float = 10`, `duration: float = 0.5` | — | 按指定强度、速度与持续时间触发屏幕震动。 |
| `StopScreenShake` | — | — | 停止当前生效的屏幕震动。 |
